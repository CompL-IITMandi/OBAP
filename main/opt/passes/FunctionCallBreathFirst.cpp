#include "FunctionCallBreathFirst.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "opt/visitors/CallBaseCounterVisitor.h"
#include <iostream>
#include <functional>
#include "utils/MNode.h"
#include "utils/RshBuiltinsMap.h"
using namespace llvm;

static void visitBBBreathFirst(llvm::BasicBlock * genesis, std::function<void(llvm::BasicBlock * bb, unsigned)> callback) {
  std::set<llvm::BasicBlock *> visitedBBs;
  std::set<llvm::BasicBlock *> worklist;
  
  worklist.insert(genesis);

  unsigned level = 0;
  while (!worklist.empty()) {
    // While the worklist exists, traverse all the basic blocks.
    for (auto & bb : worklist) {
      callback(bb, level);
      visitedBBs.insert(bb);
    }

    // Create a new worklist for visiting the next level
    std::set<llvm::BasicBlock *> next;
    for (auto & bb : worklist) {
      // successors for each bb
      for (llvm::BasicBlock *succ : successors(bb)) {
        if (visitedBBs.find(succ) == visitedBBs.end()) {
          // BB not visited before
          next.insert(succ);
        }
      }
    }
    worklist = std::move(next);
    level++;
  }
}

FunctionCallBreathFirst::Result FunctionCallBreathFirst::runOnModule(Module &M) {
  FunctionCallBreathFirst::Result res;
  for (auto & fun : M) {
    if (fun.isDeclaration()) continue;
    auto currFunName = fun.getName().str();
    auto & genesis = fun.getEntryBlock();

    std::vector<MNode> fCallData;
    MNode curr;
    unsigned workingLevel = 0;
    visitBBBreathFirst(&genesis, [&] (llvm::BasicBlock * bb, unsigned level) {
      if (workingLevel != level) {
        workingLevel = level;
        if (!curr.isEmpty()) {
          fCallData.push_back(curr);
          curr = MNode();
        }
      }
      for (auto & insn : *bb) {
        if (auto *callInsn = dyn_cast<CallBase>(&insn)) {
          auto func = callInsn->getCalledFunction();
          auto funcName = func->getName().str();
          if (RshBuiltinWeights::getWeight(funcName) > 3) {
            curr.addInstruction(callInsn);
          }
        }
      }
    });

    res.insert(std::pair<std::string, std::vector<MNode>>(currFunName, fCallData));
    
    // unsigned i = 0;
    // for (auto & ele : fCallData) {
    //   outs() << "Level: " << i++ << "\n";
    //   ele.printFunNames();
    // }
  }
  return res;
}


FunctionCallBreathFirst::Result FunctionCallBreathFirst::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  return runOnModule(M);
}

llvm::AnalysisKey FunctionCallBreathFirst::Key;