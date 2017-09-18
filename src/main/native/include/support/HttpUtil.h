/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2016. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#ifndef WPIUTIL_SUPPORT_HTTPUTIL_H_
#define WPIUTIL_SUPPORT_HTTPUTIL_H_

#include <memory>
#include <string>

#include "llvm/ArrayRef.h"
#include "llvm/SmallString.h"
#include "llvm/SmallVector.h"
#include "llvm/StringMap.h"
#include "llvm/StringRef.h"
#include "support/raw_istream.h"
#include "support/raw_socket_istream.h"
#include "support/raw_socket_ostream.h"
#include "tcpsockets/NetworkStream.h"

namespace wpi {

// Unescape a %xx-encoded URI.
// @param buf Buffer for output
// @param error Set to true if an error occurred
// @return Escaped string
llvm::StringRef UnescapeURI(llvm::StringRef str,
                            llvm::SmallVectorImpl<char>& buf, bool* error);

// Escape a string with %xx-encoding.
// @param buf Buffer for output
// @param spacePlus If true, encodes spaces to '+' rather than "%20"
// @return Escaped string
llvm::StringRef EscapeURI(llvm::StringRef str, llvm::SmallVectorImpl<char>& buf,
                          bool spacePlus = true);

// Parse a set of HTTP headers.  Saves just the Content-Type and Content-Length
// fields.
// @param is Input stream
// @param contentType If not null, Content-Type contents are saved here.
// @param contentLength If not null, Content-Length contents are saved here.
// @return False if error occurred in input stream
bool ParseHttpHeaders(wpi::raw_istream& is,
                      llvm::SmallVectorImpl<char>* contentType,
                      llvm::SmallVectorImpl<char>* contentLength);

class HttpHeaderParser {
 public:
  explicit HttpHeaderParser(bool hasStartLine) { Reset(hasStartLine); }

  // Reset the parser.  This allows reuse of internal buffers.
  // This also invalidates any StringRefs previously returned by Get()
  // functions.
  void Reset(bool hasStartLine);

  // Feed the parser with more data.
  // @param in input data
  // @return the input not consumed; empty if all input consumed
  llvm::StringRef Feed(llvm::StringRef in);

  // Returns true when all headers have been parsed.
  bool IsDone() const { return m_state == kDone; }

  // Returns true if there was an error in header parsing.
  bool HasError() const { return m_error; }

  // Get the start line.
  // Returns empty if initialized/reset with hasStartLine=false.
  llvm::StringRef GetStartLine() const { return GetBuf(m_startLine); }

  // Get the contents of a header field.
  // Returns empty if the header field was not provided.
  // This collapses folded CRLFs into single spaces.
  llvm::StringRef GetHeader(llvm::StringRef field,
                            llvm::SmallVectorImpl<char>& buf) const;

 private:
  // Internal state
  enum State { kStartLine, kHeaderLine, kDone };
  State m_state;
  bool m_error;
  size_t m_startOfLine;
  size_t m_pos;

  // These convert offset+size into m_buf data.  Can't use StringRef directly
  // as m_buf can get reallocated, invalidating the pointers.
  typedef std::pair<size_t, size_t> BufRef;
  llvm::StringRef GetBuf(const BufRef& ref) const {
    return llvm::StringRef(m_buf.data() + ref.first, ref.second);
  }
  BufRef MakeBufRef(llvm::StringRef s) const {
    BufRef v;
    v.first = s.data() - m_buf.data();
    v.second = s.size();
    return v;
  }

  BufRef m_startLine;
  struct Header {
    BufRef name;
    BufRef value;
    bool hasFold = false;
  };
  llvm::SmallVector<Header, 16> m_headers;
  std::string m_buf;
};

// Look for a MIME multi-part boundary.  On return, the input stream will
// be located at the character following the boundary (usually "\r\n").
// @param is Input stream
// @param boundary Boundary string to scan for (not including "--" prefix)
// @param saveBuf If not null, all scanned characters up to but not including
//     the boundary are saved to this string
// @return False if error occurred on input stream, true if boundary found.
bool FindMultipartBoundary(wpi::raw_istream& is, llvm::StringRef boundary,
                           std::string* saveBuf);

class HttpMultipartScanner {
 public:
  explicit HttpMultipartScanner(llvm::StringRef boundary,
                                bool saveSkipped = false) {
    Reset(boundary, saveSkipped);
  }

  // Reset the scanner.  This allows reuse of internal buffers.
  void Reset(llvm::StringRef boundary, bool saveSkipped = false);

  // Feed the scanner with more data.
  // @param in input data
  // @return the input not consumed; empty if all input consumed
  llvm::StringRef Feed(llvm::StringRef in);

  // Returns true when the boundary has been found.
  bool IsDone() const { return m_state == kDone; }

  // Get the skipped data.  Will be empty if saveSkipped was false.
  llvm::StringRef GetSkipped() const {
    return m_saveSkipped ? llvm::StringRef{m_buf} : llvm::StringRef{};
  }

 private:
  llvm::SmallString<64> m_boundary;
  bool m_saveSkipped;

  // Internal state
  enum State { kBoundary, kPadding, kDone };
  State m_state;
  size_t m_pos;

  // Buffer
  std::string m_buf;
};

class HttpLocation {
 public:
  HttpLocation() = default;
  HttpLocation(llvm::StringRef url_, bool* error, std::string* errorMsg);

  std::string url;  // retain copy
  std::string user;  // unescaped
  std::string password;  // unescaped
  std::string host;
  int port;
  std::string path;  // escaped, not including leading '/'
  std::vector<std::pair<std::string, std::string>> params;  // unescaped
  std::string fragment;
};

class HttpRequest {
 public:
  HttpRequest() = default;

  HttpRequest(const HttpLocation& loc) : host{loc.host}, port{loc.port} {
    SetPath(loc.path, loc.params);
    SetAuth(loc);
  }

  template <typename T>
  HttpRequest(const HttpLocation& loc, const T& extraParams);

  HttpRequest(const HttpLocation& loc, llvm::StringRef path_)
      : host{loc.host}, port{loc.port}, path{path_} {
    SetAuth(loc);
  }

  template <typename T>
  HttpRequest(const HttpLocation& loc, llvm::StringRef path_, const T& params)
      : host{loc.host}, port{loc.port} {
    SetPath(path_, params);
    SetAuth(loc);
  }

  llvm::SmallString<128> host;
  int port;
  std::string auth;
  llvm::SmallString<128> path;

 private:
  void SetAuth(const HttpLocation& loc);
  template <typename T>
  void SetPath(llvm::StringRef path_, const T& params);

  template <typename T>
  static llvm::StringRef GetFirst(const T& elem) { return elem.first; }
  template <typename T>
  static llvm::StringRef GetFirst(const llvm::StringMapEntry<T>& elem) {
    return elem.getKey();
  }
  template <typename T>
  static llvm::StringRef GetSecond(const T& elem) { return elem.second; }
};

class HttpConnection {
 public:
  HttpConnection(std::unique_ptr<wpi::NetworkStream> stream_, int timeout)
      : stream{std::move(stream_)}, is{*stream, timeout}, os{*stream, true} {}

  bool Handshake(const HttpRequest& request, std::string* warnMsg);

  std::unique_ptr<wpi::NetworkStream> stream;
  wpi::raw_socket_istream is;
  wpi::raw_socket_ostream os;

  // Valid after Handshake() is successful
  llvm::SmallString<64> contentType;
  llvm::SmallString<64> contentLength;

  explicit operator bool() const { return stream && !is.has_error(); }
};

}  // namespace wpi 

#include "HttpUtil.inl"

#endif  // WPIUTIL_SUPPORT_HTTPUTIL_H_
