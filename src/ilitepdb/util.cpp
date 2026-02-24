#include "ilitepdb/util.h"
#include <llvm/Support/Error.h>
#include <string>
#include <system_error>
#include <utility>

namespace mif::ilitepdb
{
std::string errorToString(llvm::Error pErr) { return llvm::toString(std::move(pErr)); }

llvm::Error makeError(std::string pMessage, std::errc pCode)
{
    return llvm::make_error<llvm::StringError>(std::move(pMessage), std::make_error_code(pCode));
}

} // namespace mif::ilitepdb
