#pragma once

#include <cstdint>
#include <filesystem>
#include <llvm/DebugInfo/CodeView/CodeView.h>
#include <llvm/DebugInfo/CodeView/GUID.h>
#include <llvm/DebugInfo/PDB/Native/RawConstants.h>
#include <llvm/DebugInfo/PDB/PDBTypes.h>
#include <llvm/Object/COFF.h>
#include <optional>
#include <string>
#include <vector>

namespace mif::ilitepdb
{

namespace fs = std::filesystem;

struct FilterRule
{
    std::optional<std::string> mWhitelistPattern;
    std::optional<std::string> mBlacklistPattern;
    bool                       mFunctionOnly  = false;
    bool                       mCaseSensitive = false;
};

struct FilterRulePatch
{
    std::optional<std::string> mWhitelistPattern;
    std::optional<std::string> mBlacklistPattern;
    std::optional<bool>        mFunctionOnly;
    std::optional<bool>        mCaseSensitive;
};

struct RunOptions
{
    fs::path   mInputPdb;
    fs::path   mOutputDir;
    fs::path   mSymbolsOutputDir;
    bool       mOverwriteExisting = true;
    FilterRule mFilter;
};

struct RunOptionsPatch
{
    std::optional<fs::path> mInputPdb;
    std::optional<fs::path> mOutputDir;
    std::optional<fs::path> mSymbolsOutputDir;
    std::optional<bool>     mOverwriteExisting;
    FilterRulePatch         mFilter;
};

struct PublicSymbol
{
    std::string                    mName;
    uint16_t                       mSection    = 0;
    uint32_t                       mOffset     = 0;
    uint32_t                       mRva        = 0;
    llvm::codeview::PublicSymFlags mFlags      = llvm::codeview::PublicSymFlags::None;
    bool                           mIsFunction = false;
};

struct PdbMetadata
{
    llvm::pdb::PdbRaw_TpiVer mTpiVersion = llvm::pdb::PdbRaw_TpiVer::PdbTpiV80;

    llvm::pdb::PdbRaw_ImplVer                 mInfoVersion   = llvm::pdb::PdbRaw_ImplVer::PdbImplVC70;
    uint32_t                                  mInfoSignature = 0;
    uint32_t                                  mInfoAge       = 1;
    llvm::codeview::GUID                      mInfoGuid      = {};
    std::vector<llvm::pdb::PdbRaw_FeatureSig> mInfoFeatures;

    llvm::pdb::PdbRaw_DbiVer mDbiVersion     = llvm::pdb::PdbRaw_DbiVer::PdbDbiV70;
    uint32_t                 mDbiAge         = 1;
    uint16_t                 mDbiBuildNumber = 0;
    uint16_t                 mDbiDllRbld     = 0;
    uint16_t                 mDbiFlags       = 0;
    uint16_t                 mDbiDllVersion  = 0;
    llvm::pdb::PDB_Machine   mDbiMachine     = llvm::pdb::PDB_Machine::Unknown;

    std::vector<llvm::object::coff_section> mSectionHeaders;
};

} // namespace mif::ilitepdb
