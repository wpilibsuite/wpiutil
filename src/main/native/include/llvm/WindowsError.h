//===-- WindowsError.h - Support for mapping windows errors to posix-------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_WINDOWSERROR_H
#define LLVM_SUPPORT_WINDOWSERROR_H

#include <system_error>

namespace wpi {
std::error_code mapWindowsError(unsigned EV);
}

#ifndef WPI_DISABLE_LLVM_SHIM
namespace llvm = wpi;
#endif

#endif
