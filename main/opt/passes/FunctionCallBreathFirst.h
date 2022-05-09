#ifndef OPT_FCBF_H
#define OPT_FCBF_H

#include "llvm/ADT/StringMap.h"
#include "llvm/Passes/PassBuilder.h"
#include "utils/MNode.h"
#include <unordered_map>
#include <set>
using namespace llvm;

// New PM implementation
struct FunctionCallBreathFirst : public llvm::AnalysisInfoMixin<FunctionCallBreathFirst> {
  using Result = llvm::StringMap<std::vector<MNode>>;

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &);

  Result runOnModule(llvm::Module &M);

  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<FunctionCallBreathFirst>;
};

#endif