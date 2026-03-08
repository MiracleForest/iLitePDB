#include "ilitepdb/options.h"
#include "ilitepdb/util.h"
#include "types.h"
#include <llvm/Support/Error.h>

namespace mif::ilitepdb
{

llvm::Expected<RunOptions> resolveRunOptions(const RunOptionsPatch& pPatch)
{
    if (!pPatch.mInputPdb.has_value())
    {
        return makeError("missing input_pdb, please set it in config file");
    }
    if (!pPatch.mOutputDir.has_value())
    {
        return makeError("missing output_dir, please set it in config file");
    }

    RunOptions resolved;
    resolved.mInputPdb               = *pPatch.mInputPdb;
    resolved.mOutputDir              = *pPatch.mOutputDir;
    resolved.mSymbolsOutputDir       = pPatch.mSymbolsOutputDir.value_or(*pPatch.mOutputDir);
    resolved.mOverwriteExisting      = pPatch.mOverwriteExisting.value_or(true);
    resolved.mFilter.mWhitelistPattern = pPatch.mFilter.mWhitelistPattern;
    resolved.mFilter.mBlacklistPattern = pPatch.mFilter.mBlacklistPattern;
    resolved.mFilter.mFunctionOnly   = pPatch.mFilter.mFunctionOnly.value_or(false);
    resolved.mFilter.mCaseSensitive  = pPatch.mFilter.mCaseSensitive.value_or(false);

    return resolved;
}

} // namespace mif::ilitepdb
