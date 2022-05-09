#ifndef UTIL_MNODE_H
#define UTIL_MNODE_H
#include "llvm/Analysis/CFG.h"
#include <sstream>
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
        llvm::outs() << *ele << "\n";
      }
    }

    std::set<std::string> getFunctionSet() {
      std::set<std::string> res;
      for (auto & ele : instructions) {
        if (llvm::CallBase * cbInst = llvm::dyn_cast<llvm::CallBase>(ele)) {
          res.insert(cbInst->getCalledFunction()->getName().str());
        }
      }
      return res;
    }

    void printFunNames() {
      for (auto & ele : instructions) {
        if (llvm::CallBase * cbInst = llvm::dyn_cast<llvm::CallBase>(ele)) {
          llvm::outs() << "  [F]" << cbInst->getCalledFunction()->getName() << " ";
        } else {
          llvm::outs() << "  " << ele->getName() << " ";
        }
      }
      llvm::outs() << "\n";
    }

    bool isEmpty() {
      return instructions.empty();
    }

    bool containsCallBaseInst() {
      for (auto & i : instructions) {
        if (llvm::isa<llvm::CallBase>(i)) {
          return true;
        }
      }
      return false;
    }

    std::string getNodeCompressedName() {
      std::stringstream ss;
      for (auto & i : instructions) {
        if (llvm::CallBase * cbInst = llvm::dyn_cast<llvm::CallBase>(i)) {
          auto func = cbInst->getCalledFunction();
          auto funcName = func->getName().str();
          ss << funcName << "_";
        }
      }
      return ss.str();
    }

    std::vector<std::string> getCalledFunctions() {
      std::vector<std::string> res;
      for (auto & i : instructions) {
        if (llvm::CallBase * cbInst = llvm::dyn_cast<llvm::CallBase>(i)) {
          auto func = cbInst->getCalledFunction();
          auto funcName = func->getName().str();
          // ss << funcName << "_";
          res.push_back(funcName);
        }
      }
      return res;
    }
};

#endif