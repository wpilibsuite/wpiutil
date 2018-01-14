/*----------------------------------------------------------------------------*/
/* Copyright (c) 2015-2018 FIRST. All Rights Reserved.                        */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#ifndef WPIUTIL_SUPPORT_BASE64_H_
#define WPIUTIL_SUPPORT_BASE64_H_

#include <cstddef>
#include <string>

#include "llvm/StringRef.h"

namespace wpi_llvm {
template <typename T>
class SmallVectorImpl;
class raw_ostream;
}  // namespace wpi_llvm

namespace wpi {

size_t Base64Decode(wpi_llvm::raw_ostream& os, wpi_llvm::StringRef encoded);

size_t Base64Decode(wpi_llvm::StringRef encoded, std::string* plain);

wpi_llvm::StringRef Base64Decode(wpi_llvm::StringRef encoded, size_t* num_read,
                             wpi_llvm::SmallVectorImpl<char>& buf);

void Base64Encode(wpi_llvm::raw_ostream& os, wpi_llvm::StringRef plain);

void Base64Encode(wpi_llvm::StringRef plain, std::string* encoded);

wpi_llvm::StringRef Base64Encode(wpi_llvm::StringRef plain,
                             wpi_llvm::SmallVectorImpl<char>& buf);

}  // namespace wpi

#endif  // WPIUTIL_SUPPORT_BASE64_H_
