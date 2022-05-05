#include "RshCallSiteCounter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "opt/visitors/CallBaseCounterVisitor.h"
#include <iostream>
using namespace llvm;

RshCallSiteCounter::Result RshCallSiteCounter::runOnModule(Module &M) {
  RshCallSiteCounter::Result res;
  CallBaseCounterVisitor cVisitor;
  cVisitor.visit(M);
  for (auto & ele : cVisitor.functionCallInfo) {
    res.insert(std::pair<std::string, unsigned>(ele.first, ele.second));
  }
  return res;
}


RshCallSiteCounter::Result RshCallSiteCounter::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
  return runOnModule(M);
}

llvm::AnalysisKey RshCallSiteCounter::Key;