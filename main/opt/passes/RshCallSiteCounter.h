#ifndef OPT_CCC_H
#define OPT_CCC_H

#include "llvm/ADT/StringMap.h"
#include "llvm/Passes/PassBuilder.h"

#include <unordered_map>
#include <set>
using namespace llvm;

// New PM implementation
struct RshCallSiteCounter : public llvm::AnalysisInfoMixin<RshCallSiteCounter> {
  using Result = llvm::StringMap<unsigned>;

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &);

  Result runOnModule(llvm::Module &M);

  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<RshCallSiteCounter>;
};

#endif