#include "ilitepdb/pdb_ops.h"
#include "ilitepdb/util.h"
#include "types.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/DebugInfo/CodeView/CodeView.h>
#include <llvm/DebugInfo/CodeView/SymbolDeserializer.h>
#include <llvm/DebugInfo/CodeView/SymbolRecord.h>
#include <llvm/DebugInfo/MSF/MSFBuilder.h>
#include <llvm/DebugInfo/PDB/IPDBSession.h>
#include <llvm/DebugInfo/PDB/Native/DbiStream.h>
#include <llvm/DebugInfo/PDB/Native/DbiStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/GSIStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/InfoStream.h>
#include <llvm/DebugInfo/PDB/Native/InfoStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/NativeSession.h>
#include <llvm/DebugInfo/PDB/Native/PDBFile.h>
#include <llvm/DebugInfo/PDB/Native/PDBFileBuilder.h>
#include <llvm/DebugInfo/PDB/Native/PublicsStream.h>
#include <llvm/DebugInfo/PDB/Native/RawConstants.h>
#include <llvm/DebugInfo/PDB/Native/SymbolStream.h>
#include <llvm/DebugInfo/PDB/PDB.h>
#include <llvm/DebugInfo/PDB/PDBTypes.h>
#include <llvm/Object/COFF.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/Error.h>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace mif::ilitepdb
{
static std::optional<uint32_t> tryResolveRva(
    llvm::pdb::NativeSession& pNativeSession,
    const uint16_t            pSection,
    const uint32_t            pOffset
)
{
    __try
    {
        return pNativeSession.getRVAFromSectOffset(pSection, pOffset);
    }
    __except (1)
    {
        return std::nullopt;
    }
}

llvm::Expected<std::unique_ptr<llvm::pdb::NativeSession>> openNativeSession(const fs::path& pInputPdb)
{
    std::unique_ptr<llvm::pdb::IPDBSession> session;
    if (auto err = llvm::pdb::loadDataForPDB(llvm::pdb::PDB_ReaderType::Native, pInputPdb.string(), session))
    {
        return std::move(err);
    }

    auto* native = dynamic_cast<llvm::pdb::NativeSession*>(session.get());
    if (!native) { return makeError("failed to open native PDB session"); }

    session.release();
    return std::unique_ptr<llvm::pdb::NativeSession>(native);
}

llvm::Expected<PdbMetadata> readPdbMetadata(llvm::pdb::NativeSession& pNativeSession)
{
    PdbMetadata metadata;
    auto&       pdbFile = pNativeSession.getPDBFile();

    auto infoStreamOrErr = pdbFile.getPDBInfoStream();
    if (!infoStreamOrErr) { return infoStreamOrErr.takeError(); }
    auto& infoStream        = *infoStreamOrErr;
    metadata.mInfoVersion   = infoStream.getVersion();
    metadata.mInfoSignature = infoStream.getSignature();
    metadata.mInfoAge       = std::max<uint32_t>(1, infoStream.getAge());
    metadata.mInfoGuid      = infoStream.getGuid();
    for (const auto feature : infoStream.getFeatureSignatures())
    {
        metadata.mInfoFeatures.push_back(feature);
    }

    auto dbiStreamOrErr = pdbFile.getPDBDbiStream();
    if (!dbiStreamOrErr) { return dbiStreamOrErr.takeError(); }
    auto& dbiStream          = *dbiStreamOrErr;
    metadata.mDbiVersion     = dbiStream.getDbiVersion();
    metadata.mDbiAge         = std::max<uint32_t>(1, dbiStream.getAge());
    metadata.mDbiBuildNumber = dbiStream.getBuildNumber();
    metadata.mDbiDllVersion  = static_cast<uint16_t>(dbiStream.getPdbDllVersion() & 0xFFFF);
    metadata.mDbiDllRbld     = dbiStream.getPdbDllRbld();
    metadata.mDbiFlags       = dbiStream.getFlags();
    metadata.mDbiMachine     = dbiStream.getMachineType();

    return metadata;
}

llvm::Expected<std::vector<PublicSymbol>> readPublicSymbols(llvm::pdb::NativeSession& pNativeSession)
{
    std::vector<PublicSymbol> symbols;
    auto&                     pdbFile = pNativeSession.getPDBFile();

    auto publicsStreamOrErr = pdbFile.getPDBPublicsStream();
    if (!publicsStreamOrErr) { return publicsStreamOrErr.takeError(); }
    auto symbolsStreamOrErr = pdbFile.getPDBSymbolStream();
    if (!symbolsStreamOrErr) { return symbolsStreamOrErr.takeError(); }

    auto& publicsStream = *publicsStreamOrErr;
    auto& symbolStream  = *symbolsStreamOrErr;

    for (const uint32_t recordOffset : publicsStream.getPublicsTable())
    {
        const auto record = symbolStream.readRecord(recordOffset);
        if (record.kind() != llvm::codeview::SymbolKind::S_PUB32) { continue; }

        auto publicRecordOrErr =
            llvm::codeview::SymbolDeserializer::deserializeAs<llvm::codeview::PublicSym32>(record);
        if (!publicRecordOrErr)
        {
            llvm::consumeError(publicRecordOrErr.takeError());
            continue;
        }

        const auto&  publicRecord = *publicRecordOrErr;
        PublicSymbol entry;
        entry.mName       = publicRecord.Name.str();
        entry.mSection    = publicRecord.Segment;
        entry.mOffset     = publicRecord.Offset;
        entry.mFlags      = publicRecord.Flags;
        entry.mIsFunction = (publicRecord.Flags & llvm::codeview::PublicSymFlags::Function)
                            != llvm::codeview::PublicSymFlags::None;
        auto rva = tryResolveRva(pNativeSession, entry.mSection, entry.mOffset);
        if (!rva.has_value()) { continue; }
        entry.mRva = *rva;
        symbols.push_back(std::move(entry));
    }

    return symbols;
}

llvm::Error writeLitePdb(
    const fs::path&                  pOutputPath,
    const PdbMetadata&               pMetadata,
    const std::vector<PublicSymbol>& pSymbols
)
{
    llvm::BumpPtrAllocator    storage;
    llvm::pdb::PDBFileBuilder builder(storage);
    if (auto err = builder.initialize(4096)) { return err; }

    for (uint32_t i = 0; i < llvm::pdb::kSpecialStreamCount; ++i)
    {
        auto stream_index_or_err = builder.getMsfBuilder().addStream(0);
        if (!stream_index_or_err) { return stream_index_or_err.takeError(); }
    }

    auto& info_builder = builder.getInfoBuilder();
    info_builder.setVersion(llvm::pdb::PdbRaw_ImplVer::PdbImplVC70);
    info_builder.setSignature(0);
    info_builder.setAge(1);
    info_builder.setGuid({});

    auto& dbi_builder = builder.getDbiBuilder();
    dbi_builder.setVersionHeader(llvm::pdb::PdbRaw_DbiVer::PdbDbiV70);
    dbi_builder.setAge(1);
    dbi_builder.setBuildNumber(14, 0);
    dbi_builder.setPdbDllVersion(0);
    dbi_builder.setPdbDllRbld(0);
    dbi_builder.setFlags(0);
    dbi_builder.setMachineType(
        pMetadata.mDbiMachine == llvm::pdb::PDB_Machine::Unknown ? llvm::pdb::PDB_Machine::Amd64
                                                                 : pMetadata.mDbiMachine
    );

    if (!pMetadata.mSectionHeaders.empty())
    {
        dbi_builder.createSectionMap(pMetadata.mSectionHeaders);
        const auto* rawData = reinterpret_cast<const uint8_t*>(pMetadata.mSectionHeaders.data());
        const auto  rawSize = pMetadata.mSectionHeaders.size() * sizeof(llvm::object::coff_section);
        llvm::ArrayRef<uint8_t> sectionData(rawData, rawSize);
        if (auto err = dbi_builder.addDbgStream(llvm::pdb::DbgHeaderType::SectionHdr, sectionData))
        {
            return err;
        }
    }

    std::vector<llvm::pdb::BulkPublic> publics;
    publics.reserve(pSymbols.size());
    for (const auto& symbol : pSymbols)
    {
        auto* nameBuffer = storage.Allocate<char>(symbol.mName.size() + 1);
        // Nauseous
        std::memcpy(nameBuffer, symbol.mName.data(), symbol.mName.size());
        nameBuffer[symbol.mName.size()] = '\0';

        llvm::pdb::BulkPublic pub;
        pub.Name    = nameBuffer;
        pub.NameLen = static_cast<uint32_t>(symbol.mName.size());
        pub.Offset  = symbol.mOffset;
        pub.Segment = symbol.mSection;
        pub.setFlags(symbol.mFlags);
        publics.push_back(pub);
    }

    auto& gsi_builder = builder.getGsiBuilder();
    gsi_builder.addPublicSymbols(std::move(publics));
    dbi_builder.setPublicsStreamIndex(gsi_builder.getPublicsStreamIndex());
    dbi_builder.setGlobalsStreamIndex(gsi_builder.getGlobalsStreamIndex());
    dbi_builder.setSymbolRecordStreamIndex(gsi_builder.getRecordStreamIndex());

    return builder.commit(pOutputPath.string(), nullptr);
}

} // namespace mif::ilitepdb
