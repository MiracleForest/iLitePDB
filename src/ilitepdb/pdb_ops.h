#pragma once

#include "ilitepdb/types.h"
#include <filesystem>
#include <llvm/DebugInfo/PDB/Native/NativeSession.h>
#include <llvm/Support/Error.h>
#include <memory>
#include <vector>

namespace mif::ilitepdb
{

llvm::Expected<std::unique_ptr<llvm::pdb::NativeSession>> openNativeSession(const fs::path& pInputPdb);

llvm::Expected<PdbMetadata> readPdbMetadata(llvm::pdb::NativeSession& pNativeSession);

llvm::Expected<std::vector<PublicSymbol>> readPublicSymbols(llvm::pdb::NativeSession& pNativeSession);

llvm::Error writeLitePdb(
    const fs::path&                  pOutputPath,
    const PdbMetadata&               pMetadata,
    const std::vector<PublicSymbol>& pSymbols
);

} // namespace mif::ilitepdb
