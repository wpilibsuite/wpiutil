/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2016. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "support/HttpUtil.h"

#include <cctype>

#include "support/Base64.h"
#include "llvm/raw_ostream.h"
#include "llvm/STLExtras.h"
#include "llvm/StringExtras.h"
#include "tcpsockets/TCPConnector.h"

namespace wpi {

llvm::StringRef UnescapeURI(llvm::StringRef str,
                            llvm::SmallVectorImpl<char>& buf, bool* error) {
  buf.clear();
  for (auto i = str.begin(), end = str.end(); i != end; ++i) {
    // pass non-escaped characters to output
    if (*i != '%') {
      // decode + to space
      if (*i == '+')
        buf.push_back(' ');
      else
        buf.push_back(*i);
      continue;
    }

    // are there enough characters left?
    if (i + 2 >= end) {
      *error = true;
      return llvm::StringRef{};
    }

    // replace %xx with the corresponding character
    unsigned val1 = llvm::hexDigitValue(*++i);
    if (val1 == -1U) {
      *error = true;
      return llvm::StringRef{};
    }
    unsigned val2 = llvm::hexDigitValue(*++i);
    if (val2 == -1U) {
      *error = true;
      return llvm::StringRef{};
    }
    buf.push_back((val1 << 4) | val2);
  }

  *error = false;
  return llvm::StringRef{buf.data(), buf.size()};
}

llvm::StringRef EscapeURI(llvm::StringRef str, llvm::SmallVectorImpl<char>& buf,
                          bool spacePlus) {
  static const char *const hexLut = "0123456789ABCDEF";

  buf.clear();
  for (auto i = str.begin(), end = str.end(); i != end; ++i) {
    // pass unreserved characters to output
    if (std::isalnum(*i) || *i == '-' || *i == '_' || *i == '.' || *i == '~') {
      buf.push_back(*i);
      continue;
    }

    // encode space to +
    if (spacePlus && *i == ' ') {
      buf.push_back('+');
      continue;
    }

    // convert others to %xx
    buf.push_back('%');
    buf.push_back(hexLut[((*i) >> 4) & 0x0f]);
    buf.push_back(hexLut[(*i) & 0x0f]);
  }

  return llvm::StringRef{buf.data(), buf.size()};
}

bool ParseHttpHeaders(raw_istream& is, llvm::SmallVectorImpl<char>* contentType,
                      llvm::SmallVectorImpl<char>* contentLength) {
  if (contentType) contentType->clear();
  if (contentLength) contentLength->clear();

  bool inContentType = false;
  bool inContentLength = false;
  llvm::SmallString<64> lineBuf;
  for (;;) {
    llvm::StringRef line = is.getline(lineBuf, 1024).rtrim();
    if (is.has_error()) return false;
    if (line.empty()) return true;  // empty line signals end of headers

    // header fields start at the beginning of the line
    if (!std::isspace(line[0])) {
      inContentType = false;
      inContentLength = false;
      llvm::StringRef field;
      std::tie(field, line) = line.split(':');
      field = field.rtrim();
      if (field == "Content-Type")
        inContentType = true;
      else if (field == "Content-Length")
        inContentLength = true;
      else
        continue;  // ignore other fields
    }

    // collapse whitespace
    line = line.ltrim();

    // save field data
    if (inContentType && contentType)
      contentType->append(line.begin(), line.end());
    else if (inContentLength && contentLength)
      contentLength->append(line.begin(), line.end());
  }
}

void HttpHeaderParser::Reset(bool hasStartLine) {
  m_state = hasStartLine ? kStartLine : kHeaderLine;
  m_error = false;
  m_startOfLine = 0;
  m_pos = 0;
  m_startLine.first = 0;
  m_startLine.second = 0;
  m_headers.resize(0);
  m_buf.resize(0);
}

llvm::StringRef HttpHeaderParser::Feed(llvm::StringRef in) {
  // Simply append the input block to the end of the internal buffer.  This
  // makes the rest of the processing a lot easier.
  if (m_state == kDone || in.empty()) return in;
  m_buf.append(in.data(), in.size());

  for (;;) {
    // Scan for end of line
    bool gotEol = false;
    while (m_pos < m_buf.size()) {
      if (m_buf[m_pos++] == '\n') {
        gotEol = true;
        break;
      }
    }

    // Didn't get a whole line yet
    if (!gotEol) return llvm::StringRef{};

    // Make a StringRef for the line to ease processing
    llvm::StringRef line(m_buf.data() + m_startOfLine, m_pos - m_startOfLine);
    m_startOfLine = m_pos;

    line = line.rtrim();

    // empty line signals end of headers
    if (line.empty()) {
      m_state = kDone;
      size_t orig_size = m_buf.size() - in.size();
      m_buf.resize(m_pos);
      return in.drop_front(m_pos - orig_size);
    }

    // handle starting line if requested
    if (m_state == kStartLine) {
      m_startLine = MakeBufRef(line);
      m_state = kHeaderLine;
      continue;
    }

    // header fields start at the beginning of the line
    llvm::StringRef field;
    if (!std::isspace(line[0])) {
      std::tie(field, line) = line.split(':');
      if (field.empty() || std::isspace(field.back())) {
        m_error = true;  // per RFC7230 section 3.2.4
        continue;  // ignore this line
      }
    }

    // remove leading whitespace
    line = line.ltrim();

    // save field data
    if (field.empty()) {
      // line folding
      if (m_headers.empty()) {
        m_error = true;
        continue;  // ignore this line
      }
      m_headers.back().hasFold = true;
      m_headers.back().value.second =
          line.end() - m_buf.data() - m_headers.back().value.first;
    } else {
      m_headers.emplace_back();
      m_headers.back().name = MakeBufRef(field);
      m_headers.back().value = MakeBufRef(line);
    }
  }
}

llvm::StringRef HttpHeaderParser::GetHeader(
    llvm::StringRef field, llvm::SmallVectorImpl<char>& buf) const {
  for (const auto& i : m_headers) {
    if (field == GetBuf(i.name)) {
      auto raw = GetBuf(i.value);
      if (!i.hasFold) return raw;

      // slow path, need to copy and collapse CRLF and surrounding whitespace
      buf.clear();
      size_t lastNonSpace = 0;
      bool removeSpace = false;
      for (auto ch : raw) {
        switch (ch) {
          case '\n':
            // remove trailing whitespace up to this point
            buf.erase(buf.begin() + lastNonSpace, buf.end());
            // remove leading whitespace
            removeSpace = true;
            // output a single space
            buf.push_back(' ');
            break;
          case '\r':
            break;  // always remove
          case '\t':
          case ' ':
            if (!removeSpace) buf.push_back(ch);
            break;
          default:
            buf.push_back(ch);
            lastNonSpace = buf.size();
            removeSpace = false;
            break;
        }
      }
      return llvm::StringRef(buf.data(), buf.size());
    }
  }

  // not found
  return llvm::StringRef{};
}

bool FindMultipartBoundary(raw_istream& is, llvm::StringRef boundary,
                           std::string* saveBuf) {
  llvm::SmallString<64> searchBuf;
  searchBuf.resize(boundary.size() + 2);
  size_t searchPos = 0;

  // Per the spec, the --boundary should be preceded by \r\n, so do a first
  // pass of 1-byte reads to throw those away (common case) and keep the
  // last non-\r\n character in searchBuf.
  if (!saveBuf) {
    do {
      is.read(searchBuf.data(), 1);
      if (is.has_error()) return false;
    } while (searchBuf[0] == '\r' || searchBuf[0] == '\n');
    searchPos = 1;
  }

  // Look for --boundary.  Read boundarysize+2 bytes at a time
  // during the search to speed up the reads, then fast-scan for -,
  // and only then match the entire boundary.  This will be slow if
  // there's a bunch of continuous -'s in the output, but that's unlikely.
  for (;;) {
    is.read(searchBuf.data() + searchPos, searchBuf.size() - searchPos);
    if (is.has_error()) return false;

    // Did we find the boundary?
    if (searchBuf[0] == '-' && searchBuf[1] == '-' &&
        searchBuf.substr(2) == boundary)
      return true;

    // Fast-scan for '-'
    size_t pos = searchBuf.find('-', searchBuf[0] == '-' ? 1 : 0);
    if (pos == llvm::StringRef::npos) {
      if (saveBuf)
        saveBuf->append(searchBuf.data(), searchBuf.size());
    } else {
      if (saveBuf)
        saveBuf->append(searchBuf.data(), pos);

      // move '-' and following to start of buffer (next read will fill)
      std::memmove(searchBuf.data(), searchBuf.data() + pos,
                   searchBuf.size() - pos);
      searchPos = searchBuf.size() - pos;
    }
  }
}

void HttpMultipartScanner::Reset(llvm::StringRef boundary, bool saveSkipped) {
  m_boundary = boundary;
  m_saveSkipped = saveSkipped;
  m_state = kBoundary;
  m_pos = 0;
  m_buf.resize(0);
}

llvm::StringRef HttpMultipartScanner::Feed(llvm::StringRef in) {
  // Simply append the input block to the end of the internal buffer.  This
  // makes the rest of the processing a lot easier.
  m_buf.append(in.data(), in.size());
  auto buf_size = m_buf.size();

  if (m_state == kBoundary) {
    auto boundary_size = m_boundary.size();
    for (; buf_size >= (m_pos + boundary_size + 3); ++m_pos) {
      auto data = m_buf.data() + m_pos;
      // Look for \n--boundary
      if (data[0] == '\n' && data[1] == '-' && data[2] == '-' &&
          llvm::StringRef(&data[3], boundary_size) == m_boundary) {
        // Found the boundary; transition to padding
        m_state = kPadding;
        m_pos += boundary_size + 3;
        break;
      }
    }
  }
 
  if (m_state == kPadding) {
    for (; buf_size >= (m_pos + 1); ++m_pos) {
      if (m_buf[m_pos] == '\n') {
        // Found the LF; return remaining input buffer (following it)
        m_state = kDone;
        ++m_pos;
        size_t orig_size = buf_size - in.size();
        m_buf.resize(m_pos);
        return in.drop_front(m_pos - orig_size);
      }
    }
  }

  // If not saving skipped, dump already-scanned buffer when it reaches a
  // threshold so it doesn't grow without bound.
  if (!m_saveSkipped && m_pos > 64000) {
    m_buf.erase(0, m_pos);
    m_pos = 0;
  }

  // We consumed the entire input
  return llvm::StringRef{};
}

HttpLocation::HttpLocation(llvm::StringRef url_, bool* error,
                           std::string* errorMsg)
    : url{url_} {
  // Split apart into components
  llvm::StringRef query{url_};

  // scheme:
  llvm::StringRef scheme;
  std::tie(scheme, query) = query.split(':');
  if (!scheme.equals_lower("http")) {
    *errorMsg = "only supports http URLs";
    *error = true;
    return;
  }

  // "//"
  if (!query.startswith("//")) {
    *errorMsg = "expected http://...";
    *error = true;
    return;
  }
  query = query.drop_front(2);

  // user:password@host:port/
  llvm::StringRef authority;
  std::tie(authority, query) = query.split('/');

  llvm::StringRef userpass, hostport;
  std::tie(userpass, hostport) = authority.split('@');
  // split leaves the RHS empty if the split char isn't present...
  if (hostport.empty()) {
    hostport = userpass;
    userpass = llvm::StringRef{};
  }

  if (!userpass.empty()) {
    llvm::StringRef rawUser, rawPassword;
    std::tie(rawUser, rawPassword) = userpass.split(':');
    llvm::SmallString<64> userBuf, passBuf;
    user = UnescapeURI(rawUser, userBuf, error);
    if (*error) {
      llvm::raw_string_ostream oss(*errorMsg);
      oss << "could not unescape user \"" << rawUser << "\"";
      oss.flush();
      return;
    }
    password = UnescapeURI(rawPassword, passBuf, error);
    if (*error) {
      llvm::raw_string_ostream oss(*errorMsg);
      oss << "could not unescape password \"" << rawPassword << "\"";
      oss.flush();
      return;
    }
  }

  llvm::StringRef portStr;
  std::tie(host, portStr) = hostport.rsplit(':');
  if (host.empty()) {
    *errorMsg = "host is empty";
    *error = true;
    return;
  }
  if (portStr.empty()) {
    port = 80;
  } else if (portStr.getAsInteger(10, port)) {
    llvm::raw_string_ostream oss(*errorMsg);
    oss << "port \"" << portStr << "\" is not an integer";
    oss.flush();
    *error = true;
    return;
  }

  // path?query#fragment
  std::tie(query, fragment) = query.split('#');
  std::tie(path, query) = query.split('?');

  // Split query string into parameters
  while (!query.empty()) {
    // split out next param and value
    llvm::StringRef rawParam, rawValue;
    std::tie(rawParam, query) = query.split('&');
    if (rawParam.empty()) continue;  // ignore "&&"
    std::tie(rawParam, rawValue) = rawParam.split('=');

    // unescape param
    *error = false;
    llvm::SmallString<64> paramBuf;
    llvm::StringRef param = UnescapeURI(rawParam, paramBuf, error);
    if (*error) {
      llvm::raw_string_ostream oss(*errorMsg);
      oss << "could not unescape parameter \"" << rawParam << "\"";
      oss.flush();
      return;
    }

    // unescape value
    llvm::SmallString<64> valueBuf;
    llvm::StringRef value = UnescapeURI(rawValue, valueBuf, error);
    if (*error) {
      llvm::raw_string_ostream oss(*errorMsg);
      oss << "could not unescape value \"" << rawValue << "\"";
      oss.flush();
      return;
    }

    params.emplace_back(std::make_pair(param, value));
  }

  *error = false;
}

void HttpRequest::SetAuth(const HttpLocation& loc) {
  if (!loc.user.empty()) {
    llvm::SmallString<64> userpass;
    userpass += loc.user;
    userpass += ':';
    userpass += loc.password;
    Base64Encode(userpass, &auth);
  }
}

bool HttpConnection::Handshake(const HttpRequest& request,
                               std::string* warnMsg) {
  // send GET request
  os << "GET /" << request.path << " HTTP/1.1\r\n";
  os << "Host: " << request.host << "\r\n";
  if (!request.auth.empty())
    os << "Authorization: Basic " << request.auth << "\r\n";
  os << "\r\n";
  os.flush();

  // read first line of response
  llvm::SmallString<64> lineBuf;
  llvm::StringRef line = is.getline(lineBuf, 1024).rtrim();
  if (is.has_error()) {
    *warnMsg = "disconnected before response";
    return false;
  }

  // see if we got a HTTP 200 response
  llvm::StringRef httpver, code, codeText;
  std::tie(httpver, line) = line.split(' ');
  std::tie(code, codeText) = line.split(' ');
  if (!httpver.startswith("HTTP")) {
    *warnMsg = "did not receive HTTP response";
    return false;
  }
  if (code != "200") {
    llvm::raw_string_ostream oss(*warnMsg);
    oss << "received " << code << " " << codeText << " response";
    oss.flush();
    return false;
  }

  // Parse headers
  if (!ParseHttpHeaders(is, &contentType, &contentLength)) {
    *warnMsg = "disconnected during headers";
    return false;
  }

  return true;
}

}  // namespace wpi
