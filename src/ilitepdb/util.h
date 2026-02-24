#pragma once

#include <llvm/Support/Error.h>
#include <string>
#include <system_error>

namespace mif::ilitepdb
{

std::string errorToString(llvm::Error pErr);

llvm::Error makeError(std::string pMessage, std::errc pCode = std::errc::invalid_argument);

} // namespace mif::ilitepdb
