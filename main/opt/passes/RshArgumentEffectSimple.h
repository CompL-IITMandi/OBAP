#ifndef OPT_RAES_H
#define OPT_RAES_H

#include "llvm/ADT/StringMap.h"
#include "llvm/Passes/PassBuilder.h"

#include <unordered_map>
#include <set>
#include <sstream>
#include "llvm/Analysis/CFG.h"
#include "utils/MNode.h"

using namespace llvm;

// New PM implementation
struct RshArgumentEffectSimple : public llvm::AnalysisInfoMixin<RshArgumentEffectSimple> {
  // For each function we will try to keep track of each argument that gets loaded from %args
  //    For each arg we will create an effect graph, this effect graph can be compared across function versions
  using Result = llvm::MapVector< const llvm::Function *, std::vector<std::pair<unsigned, std::vector<std::string>>> >;

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &);

  Result runOnModule(llvm::Module &M);

  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<RshArgumentEffectSimple>;
};

#endif