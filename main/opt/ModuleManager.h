#ifndef OPT_MODMAN_H
#define OPT_MODMAN_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"


// Include the passes
#include "opt/passes/RshCallSiteCallerCalleeInfo.h"
#include "opt/passes/RshCallSiteCounter.h"

using namespace llvm;


class ModuleManager {
  private:
    ModulePassManager MPM;
    ModuleAnalysisManager MAM;
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    PassBuilder PB;
  public:
    
    ModuleManager() {
      init();
    }

    void init() {
      PB.registerModuleAnalyses(MAM);
      PB.registerCGSCCAnalyses(CGAM);
      PB.registerFunctionAnalyses(FAM);
      PB.registerLoopAnalyses(LAM);
      PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

      // Add analysis passes
      MAM.registerPass([&] { return RshCallSiteCounter(); });
      // MAM.registerPass([&] { return RshCallSiteCallerCalleeInfo(); });

    }

    void runPasses(llvm::Module & m) {
      MPM.run(m, MAM);

      // auto result = MAM.getResult<RshCallSiteCallerCalleeInfo>(m);
      // for (auto & callSiteInfo : result) {
      //   // outs() << callSiteInfo << "\n";
      //   std::cout << "        call_site(" << callSiteInfo.first().str() << "): " << std::endl;
      //   std::cout << "          [";
      //   for (auto & caller : callSiteInfo.second.first) {
      //     std::cout << caller << " ";
      //   }
      //   std::cout << "]" << std::endl;

      //   std::cout << "          [";
      //   for (auto & callee : callSiteInfo.second.second) {
      //     std::cout << callee << " ";
      //   }
      //   std::cout << "]" << std::endl;
      // }

      auto callSiteCountRes = MAM.getResult<RshCallSiteCounter>(m);

      for (auto & ele : callSiteCountRes) {
        std::cout << "          " << ele.first().str() << ":" << ele.second << "" << std::endl;
      }

    }


};

#endif