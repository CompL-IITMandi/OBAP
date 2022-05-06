#ifndef OPT_RAT_H
#define OPT_RAT_H

#include "llvm/ADT/StringMap.h"
#include "llvm/Passes/PassBuilder.h"

#include <unordered_map>
#include <set>
#include <sstream>
#include "llvm/Analysis/CFG.h"
using namespace llvm;

class MNode {
  public:
    std::vector<llvm::Instruction *> instructions;
    bool pathToInstruction(llvm::Instruction * i) {
      for (auto & ele : instructions) {
        if (llvm::isPotentiallyReachable(ele, i)) return true;
      }

      return false;
    }

    bool formsLoop(llvm::Instruction * i) {
      for (auto & ele : instructions) {
        if (llvm::isPotentiallyReachable(ele, i) && llvm::isPotentiallyReachable(i, ele)) return true;
      }

      return false;
    }

    void addInstruction(llvm::Instruction * i) {
      instructions.push_back(i);
    }

    void mergeOtherNode(MNode other) {
      instructions.insert(instructions.end(), other.instructions.begin(), other.instructions.end());
    }

    void print() {
      for (auto & ele : instructions) {
        outs() << *ele << "\n";
      }
    }

    bool isEmpty() {
      return instructions.empty();
    }

    bool containsCallBaseInst() {
      for (auto & i : instructions) {
        if (isa<CallBase>(i)) {
          return true;
        }
      }
      return false;
    }

    std::string getNodeCompressedName() {
      std::stringstream ss;
      for (auto & i : instructions) {
        if (CallBase * cbInst = dyn_cast<CallBase>(i)) {
          auto func = cbInst->getCalledFunction();
          auto funcName = func->getName().str();
          ss << funcName << "_";
        }
      }
      return ss.str();
    }

};

// New PM implementation
struct RshArgumentTracking : public llvm::AnalysisInfoMixin<RshArgumentTracking> {
  // For each function we will try to keep track of each argument that gets loaded from %args
  //    For each arg we will create an effect graph, this effect graph can be compared across function versions
  using Result = llvm::MapVector< const llvm::Function *, std::vector<std::pair<unsigned, std::vector<MNode>>> >;

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