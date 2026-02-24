#include "ilitepdb/symbol_report.h"
#include "ilitepdb/util.h"
#include "types.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <llvm/Support/Error.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <system_error>
#include <vector>

namespace mif::ilitepdb
{
using json = nlohmann::json;

llvm::Error writeSymbolRvaReport(const fs::path& pReportPath, const std::vector<PublicSymbol>& pSymbols)
{
    json root = json::object();

    for (const auto& symbol : pSymbols)
    {
        if (!root.contains(symbol.mName))
        {
            root[symbol.mName] = symbol.mRva;
            continue;
        }

        auto& existing = root[symbol.mName];
        if (existing.is_number_unsigned())
        {
            const auto firstRva = existing.get<uint32_t>();
            existing            = json::array({ firstRva, symbol.mRva });
            continue;
        }
        existing.push_back(symbol.mRva);
    }

    std::ofstream output(pReportPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        return makeError(
            "failed to open symbol report for writing: " + pReportPath.string(),
            std::errc::io_error
        );
    }
    output << root.dump(2);
    if (!output.good())
    {
        return makeError("failed to write symbol report: " + pReportPath.string(), std::errc::io_error);
    }

    return llvm::Error::success();
}

} // namespace mif::ilitepdb
