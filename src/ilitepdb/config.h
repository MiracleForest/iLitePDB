#pragma once

#include "ilitepdb/types.h"
#include <filesystem>
#include <llvm/Support/Error.h>

namespace mif::ilitepdb
{

llvm::Expected<RunOptionsPatch> loadConfigPatch(const fs::path& pConfigPath);

} // namespace mif::ilitepdb
