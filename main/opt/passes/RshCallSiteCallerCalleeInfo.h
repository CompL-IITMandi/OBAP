#ifndef OPT_CCCI_H
#define OPT_CCCI_H

#include "llvm/ADT/StringMap.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>
#include <set>
using namespace llvm;

// New PM implementation
struct RshCallSiteCallerCalleeInfo : public llvm::AnalysisInfoMixin<RshCallSiteCallerCalleeInfo> {
  using Result = llvm::StringMap<std::pair<std::set<std::string>, std::set<std::string>>>;

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &);

  Result runOnModule(llvm::Module &M);

  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<RshCallSiteCallerCalleeInfo>;
};

#endif