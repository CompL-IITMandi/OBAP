#include "RshArgumentEffectSimple.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <set>
#include <functional>
#include "llvm/IR/CFG.h"



using namespace llvm;

// The order of load from the stack is the same as the argument number, so
// first argument in the function will have the argument populated in the index 0

RshArgumentEffectSimple::Result RshArgumentEffectSimple::runOnModule(Module &M) {
  RshArgumentEffectSimple::Result res;

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
    std::vector<std::pair< unsigned, std::vector<std::string> > > finalRes;

    auto currFun = ele.first;
    auto currArgData = ele.second;
    for (auto & data : currArgData) {
      unsigned currArg = data.first;
      llvm::Instruction * currArgMainInst = data.second;

      // List of affected instructions by the current argument
      std::set<llvm::Instruction *> affectedInstructions;

      // Populate the affected list
      recursivelyUpdateUsers(currArgMainInst, affectedInstructions);

      std::vector<std::string> calledFunctions;

      for (auto & i : affectedInstructions) {
        if (llvm::CallBase * cbInst = llvm::dyn_cast<llvm::CallBase>(i)) {
          auto func = cbInst->getCalledFunction();
          auto funcName = func->getName().str();
          calledFunctions.push_back(funcName);
        }
      }

      finalRes.push_back(std::pair<unsigned, std::vector<std::string>>(currArg, calledFunctions));
    }

    res.insert(
      std::pair<
        const llvm::Function *, 
        std::vector<std::pair<unsigned, std::vector<std::string>>>
        >(
          currFun, finalRes));

  }

  return res;
}


RshArgumentEffectSimple::Result RshArgumentEffectSimple::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  return runOnModule(M);
}

llvm::AnalysisKey RshArgumentEffectSimple::Key;