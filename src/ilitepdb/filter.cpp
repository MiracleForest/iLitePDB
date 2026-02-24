#include "ilitepdb/filter.h"
#include "ilitepdb/util.h"
#include "types.h"
#include <algorithm>
#include <exception>
#include <llvm/Support/Error.h>
#include <regex>
#include <string>
#include <vector>

namespace mif::ilitepdb
{

llvm::Expected<CompiledFilter> compileFilter(const FilterRule& pRule)
{
    CompiledFilter compiled;
    compiled.mFunctionOnly = pRule.mFunctionOnly;

    const auto flags =
        pRule.mCaseSensitive ? std::regex::ECMAScript : (std::regex::ECMAScript | std::regex::icase);
    try
    {
        if (pRule.mIncludePattern.has_value())
        {
            compiled.mIncludeRegex = std::regex(*pRule.mIncludePattern, flags);
        }
    }
    catch (const std::exception& pEx)
    {
        return makeError("invalid regex in filter: " + std::string(pEx.what()));
    }

    return compiled;
}

bool passesFilter(const PublicSymbol& pSymbol, const CompiledFilter& pFilter)
{
    if (pSymbol.mName.empty() || pSymbol.mSection == 0) { return false; }
    if (pFilter.mFunctionOnly && !pSymbol.mIsFunction) { return false; }
    if (pFilter.mIncludeRegex.has_value() && !std::regex_search(pSymbol.mName, *pFilter.mIncludeRegex))
    {
        return false;
    }
    return true;
}

void sortAndDeduplicate(std::vector<PublicSymbol>& pSymbols)
{
    std::sort(pSymbols.begin(), pSymbols.end(), [](const PublicSymbol& pLhs, const PublicSymbol& pRhs) {
        if (pLhs.mRva != pRhs.mRva) { return pLhs.mRva < pRhs.mRva; }
        if (pLhs.mSection != pRhs.mSection) { return pLhs.mSection < pRhs.mSection; }
        if (pLhs.mOffset != pRhs.mOffset) { return pLhs.mOffset < pRhs.mOffset; }
        return pLhs.mName < pRhs.mName;
    });

    pSymbols.erase(
        std::unique(
            pSymbols.begin(),
            pSymbols.end(),
            [](const PublicSymbol& pLhs, const PublicSymbol& pRhs) {
                return pLhs.mSection == pRhs.mSection && pLhs.mOffset == pRhs.mOffset
                       && pLhs.mName == pRhs.mName;
            }
        ),
        pSymbols.end()
    );
}

} // namespace mif::ilitepdb
