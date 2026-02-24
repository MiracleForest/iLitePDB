#pragma once

#include "ilitepdb/types.h"
#include <filesystem>
#include <llvm/Support/Error.h>
#include <vector>

namespace mif::ilitepdb
{
llvm::Error writeSymbolRvaReport(const fs::path& pReportPath, const std::vector<PublicSymbol>& pSymbols);
} // namespace mif::ilitepdb
