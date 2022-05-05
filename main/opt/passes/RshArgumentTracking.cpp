#include "RshArgumentTracking.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <set>
using namespace llvm;

// The order of load from the stack is the same as the argument number, so
// first argument in the function will have the argument populated in the index 0

RshArgumentTracking::Result RshArgumentTracking::runOnModule(Module &M) {
  RshArgumentTracking::Result res;

  std::unordered_map<unsigned, llvm::Value *> argValMap;

  for (auto & fun : M) {
    // Only interested in function that are defined in this module
    if (fun.isDeclaration()) continue;
    for (auto currArg = fun.arg_begin(); currArg != fun.arg_end(); ++currArg) {
      if (currArg->getName().str() == "args") {
        // outs() << "Arg: " << currArg->getName().str() << "\n";
        for (llvm::User *U : currArg->users()) {
          // skip case, if the user is a deopt site, then we can skip it
          if (CallBase * cbInst = dyn_cast<CallBase>(U)) {
            auto calledFun = cbInst->getCalledFunction()->getName().str();
            // outs() << "  User[D]" << calledFun << "\n";
          } else {
            if (GetElementPtrInst * gepInst = dyn_cast<GetElementPtrInst>(U)) {
              // outs() << "  GEPInst: " << *U << "\n";
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
                argValMap[argIndex] = U;
              }

            } else {
              errs() << "UNKInst\n";
              errs() << "  ->" << *U << "\n";
            }
          }
        }
      }
    }
  }

  std::function<void(llvm::Value *, std::set<llvm::Value *> &)> recursivelyUpdateUsers = [&] (llvm::Value * currVal, std::set<llvm::Value *> & alreadyAdded) {
    
    if (alreadyAdded.find(currVal) == alreadyAdded.end()) {
      // get the uses for the currentValue
      alreadyAdded.insert(currVal);

      for (llvm::User *user : currVal->users()) {
        recursivelyUpdateUsers(user, alreadyAdded);
      }

    }
  };

  for (auto & ele : argValMap) {
    outs() << "Processing Arg: " << ele.first << "\n";
    outs() << "        VAL AT: " << *ele.second << "\n";
    std::set<llvm::Value *> affected;
    recursivelyUpdateUsers(ele.second, affected);
    for (auto & ele : affected) {
      outs() << "          USER: " << *ele << "\n";
    }

  }

  return res;
}


RshArgumentTracking::Result RshArgumentTracking::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  return runOnModule(M);
}

llvm::AnalysisKey RshArgumentTracking::Key;