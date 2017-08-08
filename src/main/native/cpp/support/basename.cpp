/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2017. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "support/basename.h"

#ifdef _WIN32
#include <stdlib.h>
#else
#include <libgen.h>
#endif

#include "llvm/SmallString.h"
#include "llvm/raw_ostream.h"

namespace wpi {

std::string Basename(llvm::StringRef path) {
  std::string str;
  llvm::raw_string_ostream oss(str);
  WriteBasename(oss, path);
  oss.flush();
  return str;
}

llvm::StringRef Basename(llvm::StringRef path,
                         llvm::SmallVectorImpl<char>& buf) {
  llvm::raw_svector_ostream oss(buf);
  WriteBasename(oss, path);
  return oss.str();
}

llvm::raw_ostream& WriteBasename(llvm::raw_ostream& os, llvm::StringRef path) {
  llvm::SmallString<128> pathbuf;
#ifdef _WIN32
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];
  _splitpath_s(path.c_str(pathbuf), nullptr, 0, nullptr, 0, fname, _MAX_FNAME,
               ext, _MAX_EXT);
  os << fname << ext;
#else
  // must make a copy
  pathbuf = path;
  pathbuf.push_back('\0');
  os << basename(pathbuf.data());
#endif
  return os;
}

}  // namespace wpi
