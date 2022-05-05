#include "RshCallSiteCallerCalleeInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
using namespace llvm;

static bool isCallSiteArg(std::string arg) {
  return arg.substr(0,5) == "pluu_";
}

static std::string getHastString(std::string globalName) {
  auto currPref = globalName.substr(0, 5);
  auto n = globalName;
  if (currPref == "code_") {
    auto firstDel = n.find('_');
    auto secondDel = n.find('_', firstDel + 1);
    auto hast = n.substr(firstDel + 1, secondDel - firstDel - 1);
    return hast;
  } else if (currPref == "vtab_") {
    auto firstDel = n.find('_');
    auto secondDel = n.find('_', firstDel + 1);
    auto hast = n.substr(firstDel + 1, secondDel - firstDel - 1);
    return hast;
  } else if (currPref == "clos_") {
    auto firstDel = n.find('_');
    auto hast = n.substr(firstDel + 1);
    return hast;
  } else if (currPref == "optd_") {
    auto firstDel = n.find('_');
    auto secondDel = n.find('_', firstDel + 1);
    auto hast = n.substr(firstDel + 1, secondDel - firstDel - 1);
    return hast;
  } else if (currPref == "pluu_") {
    auto firstDel = n.find('_');
    auto hast = n.substr(firstDel + 1);
    return hast;
  } else {
    return "NHT";
  }
}

RshCallSiteCallerCalleeInfo::Result RshCallSiteCallerCalleeInfo::runOnModule(Module &M) {
  RshCallSiteCallerCalleeInfo::Result res;
  std::unordered_map<std::string, std::pair<std::set<std::string>, std::set<std::string>>> resMap;

  auto addCallSiteData = [&] (std::string functionName, std::set<std::string> c_callees, std::set<std::string> c_callers) {
    if (resMap.find(functionName) == resMap.end()) {
      resMap[functionName] = std::pair<std::set<std::string>, std::set<std::string>>();
    }

    auto & callers = resMap[functionName].first;
    auto & callees = resMap[functionName].second;

    for (auto & ele : c_callees) {
      callees.insert(ele);
    }

    for (auto & ele : c_callers) {
      callers.insert(ele);
    }

  };

  for (auto & fun : M) {
    for (auto & BB : fun) {
      for (auto & insn : BB) {
        if (auto *callInsn = dyn_cast<CallBase>(&insn)) {
          auto func = callInsn->getCalledFunction();
          auto funcName = func->getName().str();

          std::set<std::string> globals;
          std::set<std::string> callSites;

          for (auto a = callInsn->arg_begin(); a != callInsn->arg_end(); ++a) {
            // Either a global or loads a global value
            if (isa<GlobalVariable>(a->get())) {
              std::string globalName = a->get()->getName().str();
              
              if (isCallSiteArg(globalName)) {
                callSites.insert(getHastString(globalName));
              } else {
                globals.insert(getHastString(globalName));
              }
            } else if (auto *loadInsn = dyn_cast<LoadInst>(a->get())) {
              auto operand = loadInsn->getPointerOperand();
              // If the operand is a global value then we are interested in it.
              if (isa<GlobalVariable>(operand)) {
                std::string globalName = operand->getName().str();
                if (isCallSiteArg(globalName)) {
                  callSites.insert(getHastString(globalName));
                  
                } else {
                  globals.insert(getHastString(globalName));
                  
                }
              }
            }
          }

          addCallSiteData(funcName, globals, callSites);
        }
      }
    }
  }

  // std::cout << "Processed module:" << std::endl;
  for (auto & ele : resMap) {
    res.insert(std::pair<std::string, std::pair<std::set<std::string>, std::set<std::string>>>(ele.first, ele.second));
  }
  return res;
}


RshCallSiteCallerCalleeInfo::Result RshCallSiteCallerCalleeInfo::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  return runOnModule(M);
}

llvm::AnalysisKey RshCallSiteCallerCalleeInfo::Key;