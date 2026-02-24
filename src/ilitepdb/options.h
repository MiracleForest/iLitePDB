#pragma once

#include "ilitepdb/types.h"
#include <llvm/Support/Error.h>

namespace mif::ilitepdb
{

llvm::Expected<RunOptions> resolveRunOptions(const RunOptionsPatch& pPatch);

} // namespace mif::ilitepdb
