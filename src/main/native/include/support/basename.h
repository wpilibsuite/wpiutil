/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2017. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#ifndef WPIUTIL_SUPPORT_BASENAME_H_
#define WPIUTIL_SUPPORT_BASENAME_H_

#include <string>

#include "llvm/SmallVector.h"
#include "llvm/StringRef.h"

namespace llvm {
class raw_ostream;
}

namespace wpi {

/**
 * Get the filename part of the path.  This is defined as the last component
 * of the path (e.g. "/" is returned for a path of "/").
 * @param path Path
 * @return filename
 */
std::string Basename(llvm::StringRef path);

llvm::StringRef Basename(llvm::StringRef path,
                         llvm::SmallVectorImpl<char>& buf);
llvm::raw_ostream& WriteBasename(llvm::raw_ostream& os, llvm::StringRef path);

}  // namespace wpi

#endif  // WPIUTIL_SUPPORT_BASENAME_H_
