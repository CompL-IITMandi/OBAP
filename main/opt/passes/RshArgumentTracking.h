#ifndef OPT_RAT_H
#define OPT_RAT_H

#include "llvm/ADT/StringMap.h"
#include "llvm/Passes/PassBuilder.h"

#include <unordered_map>
#include <set>
using namespace llvm;

// New PM implementation
struct RshArgumentTracking : public llvm::AnalysisInfoMixin<RshArgumentTracking> {
  // For each function we will try to keep track of each argument that gets loaded from %args
  //    For each arg we will create an effect graph, this effect graph can be compared across function versions
  using Result = llvm::MapVector< const llvm::Function *, std::unordered_map<unsigned, std::vector<std::string>> >;

  Result run(llvm::Module &M, llvm::ModuleAnalysisManager &);

  Result runOnModule(llvm::Module &M);

  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<RshArgumentTracking>;
};

#endif