#include "ilitepdb/config.h"
#include "ilitepdb/filter.h"
#include "ilitepdb/options.h"
#include "ilitepdb/pdb_ops.h"
#include "ilitepdb/symbol_report.h"
#include "ilitepdb/types.h"
#include "ilitepdb/util.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/core.h>
#include <llvm/Support/Error.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace mif::ilitepdb;

static bool isPdbFilePath(const fs::path& pPath)
{
    std::string extension = pPath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char pCh) {
        return static_cast<char>(std::tolower(pCh));
    });
    return extension == ".pdb";
}

static llvm::Expected<fs::path> resolveInputPdbPath(const fs::path& pInputPath)
{
    if (!fs::exists(pInputPath))
    {
        return makeError(
            "input path does not exist: " + pInputPath.string(),
            std::errc::no_such_file_or_directory
        );
    }

    if (fs::is_regular_file(pInputPath))
    {
        if (!isPdbFilePath(pInputPath))
        {
            return makeError("input file extension must be .pdb: " + pInputPath.string());
        }
        return pInputPath;
    }

    if (!fs::is_directory(pInputPath))
    {
        return makeError("input path must be a .pdb file or a directory: " + pInputPath.string());
    }

    std::vector<fs::path> pdbFiles;
    std::error_code       ec;
    for (const auto& entry : fs::directory_iterator(pInputPath, ec))
    {
        if (ec)
        {
            return makeError("failed to iterate input directory: " + ec.message(), std::errc::io_error);
        }
        if (!entry.is_regular_file(ec))
        {
            if (ec)
            {
                return makeError(
                    "failed to read entry in input directory: " + ec.message(),
                    std::errc::io_error
                );
            }
            continue;
        }
        if (isPdbFilePath(entry.path())) { pdbFiles.push_back(entry.path()); }
    }
    if (ec) { return makeError("failed to iterate input directory: " + ec.message(), std::errc::io_error); }
    if (pdbFiles.empty())
    {
        return makeError("input directory does not contain a .pdb file: " + pInputPath.string());
    }
    if (pdbFiles.size() > 1)
    {
        return makeError("input directory must contain exactly one .pdb file: " + pInputPath.string());
    }
    return pdbFiles.front();
}

static llvm::Error validateFilesystemOptions(RunOptions& pOptions)
{
    auto resolvedInputPdbOrErr = resolveInputPdbPath(pOptions.mInputPdb);
    if (!resolvedInputPdbOrErr) { return resolvedInputPdbOrErr.takeError(); }
    pOptions.mInputPdb = *resolvedInputPdbOrErr;

    std::error_code ec;
    fs::create_directories(pOptions.mOutputDir, ec);
    if (ec) { return makeError("failed to create output directory: " + ec.message(), std::errc::io_error); }
    fs::create_directories(pOptions.mSymbolsOutputDir, ec);
    if (ec)
    {
        return makeError("failed to create symbols output directory: " + ec.message(), std::errc::io_error);
    }

    const fs::path outputPath = pOptions.mOutputDir / pOptions.mInputPdb.filename();
    if (fs::equivalent(pOptions.mInputPdb, outputPath, ec) && !ec)
    {
        return makeError("input path and output path cannot be the same");
    }

    return llvm::Error::success();
}

static llvm::Error prepareOutputFilePath(const fs::path& pPath, const bool pOverwriteExisting)
{
    std::error_code ec;
    if (!fs::exists(pPath, ec))
    {
        if (ec)
        {
            return makeError("failed to check output file path: " + ec.message(), std::errc::io_error);
        }
        return llvm::Error::success();
    }
    if (!pOverwriteExisting)
    {
        return makeError("output file already exists: " + pPath.string(), std::errc::file_exists);
    }
    if (!fs::is_regular_file(pPath, ec))
    {
        if (ec)
        {
            return makeError("failed to inspect output file path: " + ec.message(), std::errc::io_error);
        }
        return makeError("output path exists and is not a regular file: " + pPath.string());
    }
    if (!fs::remove(pPath, ec) || ec)
    {
        return makeError("failed to overwrite existing file: " + pPath.string(), std::errc::io_error);
    }
    return llvm::Error::success();
}


int main(int, char* pArgv[])
{
    auto configPatchOrErr = loadConfigPatch(fs::path(pArgv[0]).parent_path() / "ilitepdb.json");
    if (!configPatchOrErr)
    {
        fmt::print(
            stderr,
            "配置文件错误({}): {}\n",
            (fs::path(pArgv[0]).parent_path() / "ilitepdb.json").string(),
            errorToString(configPatchOrErr.takeError())
        );
        fmt::print(stderr, "请先编辑配置文件后再运行。\n");
        return 1;
    }

    auto optionsOrErr = resolveRunOptions(*configPatchOrErr);
    if (!optionsOrErr)
    {
        fmt::print(stderr, "配置文件参数错误: {}\n", errorToString(optionsOrErr.takeError()));
        return 1;
    }
    RunOptions options = *optionsOrErr;

    if (auto err = validateFilesystemOptions(options))
    {
        fmt::print(stderr, "路径检查失败: {}\n", errorToString(std::move(err)));
        return 1;
    }

    auto filterOrErr = compileFilter(options.mFilter);
    if (!filterOrErr)
    {
        fmt::print(stderr, "筛选规则错误: {}\n", errorToString(filterOrErr.takeError()));
        return 1;
    }
    const CompiledFilter filter = *filterOrErr;

    auto nativeSessionOrErr = openNativeSession(options.mInputPdb);
    if (!nativeSessionOrErr)
    {
        fmt::print(stderr, "读取 PDB 失败: {}\n", errorToString(nativeSessionOrErr.takeError()));
        return 1;
    }
    auto nativeSession = std::move(*nativeSessionOrErr);

    auto metadataOrErr = readPdbMetadata(*nativeSession);
    if (!metadataOrErr)
    {
        fmt::print(stderr, "读取 PDB 元数据失败: {}\n", errorToString(metadataOrErr.takeError()));
        return 1;
    }
    const PdbMetadata metadata = *metadataOrErr;

    auto symbolsOrErr = readPublicSymbols(*nativeSession);
    if (!symbolsOrErr)
    {
        fmt::print(stderr, "读取符号失败: {}\n", errorToString(symbolsOrErr.takeError()));
        return 1;
    }
    const auto& allSymbols = *symbolsOrErr;

    std::vector<PublicSymbol> filteredSymbols;
    filteredSymbols.reserve(allSymbols.size());
    for (const auto& symbol : allSymbols)
    {
        if (passesFilter(symbol, filter)) { filteredSymbols.push_back(symbol); }
    }
    sortAndDeduplicate(filteredSymbols);

    if (filteredSymbols.empty())
    {
        fmt::print(stderr, "筛选后没有可写入的符号。\n");
        return 1;
    }

    const fs::path outputPath = options.mOutputDir / options.mInputPdb.filename();
    if (auto err = prepareOutputFilePath(outputPath, options.mOverwriteExisting))
    {
        fmt::print(stderr, "输出文件检查失败: {}\n", errorToString(std::move(err)));
        return 1;
    }
    if (auto err = writeLitePdb(outputPath, metadata, filteredSymbols))
    {
        fmt::print(stderr, "写入新 PDB 失败: {}\n", errorToString(std::move(err)));
        return 1;
    }
    const fs::path symbolReportPath =
        options.mSymbolsOutputDir / (outputPath.stem().string() + ".symbols.json");
    if (auto err = prepareOutputFilePath(symbolReportPath, options.mOverwriteExisting))
    {
        fmt::print(stderr, "符号列表文件检查失败: {}\n", errorToString(std::move(err)));
        return 1;
    }
    if (auto err = writeSymbolRvaReport(symbolReportPath, filteredSymbols))
    {
        fmt::print(stderr, "写入符号 RVA 列表失败: {}\n", errorToString(std::move(err)));
        return 1;
    }

    fmt::print("输入符号: {}\n", allSymbols.size());
    fmt::print("保留符号: {}\n", filteredSymbols.size());
    fmt::print("输出文件: {}\n", outputPath.string());
    fmt::print("符号列表: {}\n", symbolReportPath.string());
    fmt::print("示例符号:\n");
    const size_t previewCount = std::min<size_t>(5, filteredSymbols.size());
    for (size_t i = 0; i < previewCount; ++i)
    {
        const auto& symbol = filteredSymbols[i];
        fmt::print(
            "  {:>2}. RVA=0x{:08X}  {}:{}  {}\n",
            i + 1,
            symbol.mRva,
            symbol.mSection,
            symbol.mOffset,
            symbol.mName
        );
    }

    return 0;
}
