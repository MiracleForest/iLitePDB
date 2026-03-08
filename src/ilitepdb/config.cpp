#include "ilitepdb/config.h"
#include "ilitepdb/util.h"
#include "types.h"
#include <exception>
#include <filesystem>
#include <fstream>
#include <llvm/Support/Error.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace mif::ilitepdb
{
using json = nlohmann::json;

static llvm::Error ensureAllowedKeys(
    const json&                            pObject,
    const std::unordered_set<std::string>& pAllowed,
    const std::string_view                 pContextName
)
{
    for (auto it = pObject.begin(); it != pObject.end(); ++it)
    {
        if (!pAllowed.contains(it.key()))
        {
            return makeError("unknown key in " + std::string(pContextName) + ": " + it.key());
        }
    }
    return llvm::Error::success();
}

static llvm::Error parseFilterObject(const json& pFilters, FilterRulePatch& pPatch)
{
    if (!pFilters.is_object()) { return makeError("filters must be a JSON object"); }
    static const std::unordered_set<std::string> kAllowed {
        "whitelist",
        "blacklist",
        "function_only",
        "case_sensitive"
    };
    if (auto err = ensureAllowedKeys(pFilters, kAllowed, "filters")) { return err; }

    if (pFilters.contains("whitelist"))
    {
        if (!pFilters["whitelist"].is_string()) { return makeError("filters.whitelist must be a string"); }
        pPatch.mWhitelistPattern = pFilters["whitelist"].get<std::string>();
    }
    if (pFilters.contains("blacklist"))
    {
        if (!pFilters["blacklist"].is_string()) { return makeError("filters.blacklist must be a string"); }
        pPatch.mBlacklistPattern = pFilters["blacklist"].get<std::string>();
    }
    if (pFilters.contains("function_only"))
    {
        if (!pFilters["function_only"].is_boolean())
        {
            return makeError("filters.function_only must be a boolean");
        }
        pPatch.mFunctionOnly = pFilters["function_only"].get<bool>();
    }
    if (pFilters.contains("case_sensitive"))
    {
        if (!pFilters["case_sensitive"].is_boolean())
        {
            return makeError("filters.case_sensitive must be a boolean");
        }
        pPatch.mCaseSensitive = pFilters["case_sensitive"].get<bool>();
    }

    return llvm::Error::success();
}

llvm::Expected<RunOptionsPatch> loadConfigPatch(const fs::path& pConfigPath)
{
    std::ifstream input(pConfigPath, std::ios::in | std::ios::binary);
    if (!input.is_open())
    {
        return makeError(
            "failed to open config file: " + pConfigPath.string(),
            std::errc::no_such_file_or_directory
        );
    }

    json root;
    try
    {
        input >> root;
    }
    catch (const std::exception& pEx)
    {
        return makeError("failed to parse JSON config: " + std::string(pEx.what()));
    }

    if (!root.is_object()) { return makeError("config root must be a JSON object"); }

    static const std::unordered_set<std::string> kAllowed { "input_pdb",
                                                            "output_dir",
                                                            "symbols_output_dir",
                                                            "overwrite_existing",
                                                            "filters" };
    if (auto err = ensureAllowedKeys(root, kAllowed, "config")) { return err; }

    RunOptionsPatch patch;
    if (root.contains("input_pdb"))
    {
        if (!root["input_pdb"].is_string()) { return makeError("input_pdb must be a string"); }
        patch.mInputPdb = fs::path(root["input_pdb"].get<std::string>());
    }
    if (root.contains("output_dir"))
    {
        if (!root["output_dir"].is_string()) { return makeError("output_dir must be a string"); }
        patch.mOutputDir = fs::path(root["output_dir"].get<std::string>());
    }
    if (root.contains("symbols_output_dir"))
    {
        if (!root["symbols_output_dir"].is_string())
        {
            return makeError("symbols_output_dir must be a string");
        }
        patch.mSymbolsOutputDir = fs::path(root["symbols_output_dir"].get<std::string>());
    }
    if (root.contains("overwrite_existing"))
    {
        if (!root["overwrite_existing"].is_boolean())
        {
            return makeError("overwrite_existing must be a boolean");
        }
        patch.mOverwriteExisting = root["overwrite_existing"].get<bool>();
    }
    if (root.contains("filters"))
    {
        if (auto err = parseFilterObject(root["filters"], patch.mFilter)) { return err; }
    }

    return patch;
}

} // namespace mif::ilitepdb
