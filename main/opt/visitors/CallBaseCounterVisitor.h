#ifndef CALLBASE_INST_VISIT_H
#define CALLBASE_INST_VISIT_H

#include "llvm/IR/InstVisitor.h"
#include <unordered_map>
struct CallBaseCounterVisitor : public llvm::InstVisitor<CallBaseCounterVisitor> {
  unsigned count;
  std::unordered_map<std::string, unsigned> functionCallInfo;
  CallBaseCounterVisitor() : count(0) {}
  void visitCallBase(llvm::CallBase &inst);
};


#endif