#include "RshArgumentTracking.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <set>
#include <functional>
#include "llvm/IR/CFG.h"



using namespace llvm;

// The order of load from the stack is the same as the argument number, so
// first argument in the function will have the argument populated in the index 0

RshArgumentTracking::Result RshArgumentTracking::runOnModule(Module &M) {
  RshArgumentTracking::Result res;

  std::unordered_map<const llvm::Function *, std::unordered_map<unsigned, llvm::Instruction *>> funArgMap;

  for (auto & fun : M) {
    std::unordered_map<unsigned, llvm::Instruction *> argValMap;
    // Only interested in functions that are defined in this module
    if (fun.isDeclaration()) continue;
    for (auto currArg = fun.arg_begin(); currArg != fun.arg_end(); ++currArg) {
      if (currArg->getName().str() == "args") {
        for (llvm::User *U : currArg->users()) {
          // skip case, if the user is a deopt site, then we can skip it
          if (CallBase * cbInst = dyn_cast<CallBase>(U)) {
            auto calledFun = cbInst->getCalledFunction()->getName().str();
          } else {
            if (GetElementPtrInst * gepInst = dyn_cast<GetElementPtrInst>(U)) {
              // Get element pointer instruction, check if the instruction is trying to load an argument
              int count = std::distance(gepInst->idx_begin(), gepInst->idx_end());
              if (count != 2) {
                errs() << "count for the GEP instruction is less than expected.\n";
                errs() << "  ->" << *gepInst << "\n";
              }

              ConstantInt * val = dyn_cast<llvm::ConstantInt>(gepInst->idx_begin()->get());
              auto argIndex = val->getZExtValue();
              
              if (U->user_empty()) {
                errs() << "No user for the inst.\n";
                errs() << "  ->" << *U << "\n";
              } else {
                argValMap[argIndex] = cast<Instruction>(U);
              }

            } else {
              errs() << "UNKInst\n";
              errs() << "  ->" << *U << "\n";
            }
          }
        }
      }
    }
    funArgMap[&fun] = argValMap;
  }

  std::function<void(llvm::Instruction *, std::set<llvm::Instruction *> &)> recursivelyUpdateUsers = [&] (llvm::Instruction * currInst, std::set<llvm::Instruction *> & alreadyAdded) {
    if (alreadyAdded.find(currInst) == alreadyAdded.end()) {
      // get the uses for the currentValue
      alreadyAdded.insert(currInst);

      for (llvm::User *user : currInst->users()) {
        if (llvm::Instruction * i = dyn_cast<llvm::Instruction>(user)) {
          recursivelyUpdateUsers(i, alreadyAdded);
        }
      }
    }
  };

  

  // For each function, find the argument effects
  for (auto & ele : funArgMap) {
    std::vector<std::pair< unsigned, std::vector<MNode> > > finalRes;

    auto currFun = ele.first;
    auto currArgData = ele.second;
    for (auto & data : currArgData) {
      unsigned currArg = data.first;
      llvm::Instruction * currArgMainInst = data.second;

      // List of affected instructions by the current argument
      std::set<llvm::Instruction *> affectedInstructions;

      // Populate the affected list
      recursivelyUpdateUsers(currArgMainInst, affectedInstructions);

      // Remove the main inst from the affected list
      affectedInstructions.erase(currArgMainInst);

      // Create levelCompressedSummary
      std::vector<MNode> levelCompressedSummary;
      MNode genesis;
      genesis.addInstruction(currArgMainInst);
      levelCompressedSummary.push_back(genesis);
      
      llvm::BasicBlock * genesisBasicBlock = currArgMainInst->getParent();
      // First process all statements in the genesis basic block simply
      for (auto & i : *genesisBasicBlock) {
        if (affectedInstructions.find(&i) != affectedInstructions.end()) {
          // if it exists remove it from the affected instructions list
          affectedInstructions.erase(&i);
          MNode curr;
          curr.addInstruction(&i);
          if (!curr.isEmpty() && curr.containsCallBaseInst()) levelCompressedSummary.push_back(curr);
        }
      }


      std::set<llvm::BasicBlock *> visitedBBs;
      visitedBBs.insert(genesisBasicBlock);


      std::function<void(std::set<llvm::BasicBlock *> &)> processWorklistRecursively = [&] (std::set<llvm::BasicBlock *> & currWorklist) {
        MNode currLevelNode;
        std::set<llvm::BasicBlock *> nextWorklist;

        for (auto & currBBAtLevel : currWorklist) {
          visitedBBs.insert(currBBAtLevel);
          // iterate over instructions of the current basic block
          for (auto & i : *currBBAtLevel) {
            if (affectedInstructions.find(&i) != affectedInstructions.end()) {
              // if it exists remove it from the affected instructions list
              affectedInstructions.erase(&i);
              currLevelNode.addInstruction(&i);
            }
          }
          // Add successors to the nextWorklist that should be processed next
          for (llvm::BasicBlock *succ : successors(currBBAtLevel)) {
            if (visitedBBs.find(succ) == visitedBBs.end()) {
              // BB not visited before
              nextWorklist.insert(succ);
            }
          }
        }
        if (!currLevelNode.isEmpty() && currLevelNode.containsCallBaseInst()) levelCompressedSummary.push_back(currLevelNode);

        if (!nextWorklist.empty()) processWorklistRecursively(nextWorklist);
      };

      std::set<llvm::BasicBlock *> initialWorklist;

      // initialize initial worklist
      for (llvm::BasicBlock *succ : successors(genesisBasicBlock)) {
        initialWorklist.insert(succ);
      }

      processWorklistRecursively(initialWorklist);

      finalRes.push_back(std::pair<unsigned, std::vector<MNode>>(currArg, levelCompressedSummary));

    }

    res.insert(
      std::pair<
        const llvm::Function *, 
        std::vector<std::pair<unsigned, std::vector<MNode>>>
        >(
          currFun, finalRes));

  }

  return res;
}


RshArgumentTracking::Result RshArgumentTracking::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  return runOnModule(M);
}

llvm::AnalysisKey RshArgumentTracking::Key;