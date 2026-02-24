#pragma once

#include "ilitepdb/types.h"
#include <llvm/Support/Error.h>
#include <optional>
#include <regex>
#include <vector>

namespace mif::ilitepdb
{

struct CompiledFilter
{
    std::optional<std::regex> mIncludeRegex;
    bool                      mFunctionOnly = false;
};

llvm::Expected<CompiledFilter> compileFilter(const FilterRule& pRule);

bool passesFilter(const PublicSymbol& pSymbol, const CompiledFilter& pFilter);

void sortAndDeduplicate(std::vector<PublicSymbol>& pSymbols);

} // namespace mif::ilitepdb
