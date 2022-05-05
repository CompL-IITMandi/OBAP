#include "CallBaseCounterVisitor.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void CallBaseCounterVisitor::visitCallBase(CallBase &inst) {
  auto funName = inst.getCalledFunction()->getName().str();
  // not sure if this is needed?
  if (functionCallInfo.find(funName) == functionCallInfo.end()) {
    functionCallInfo[funName] = 1;
  }
  functionCallInfo[funName]++;
  count++;
}