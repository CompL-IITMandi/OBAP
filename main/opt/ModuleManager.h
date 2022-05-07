#ifndef OPT_MODMAN_H
#define OPT_MODMAN_H

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"


// Include the passes
#include "opt/passes/RshCallSiteCallerCalleeInfo.h"
#include "opt/passes/RshCallSiteCounter.h"
#include "opt/passes/RshArgumentTracking.h"
#include "opt/passes/RshArgumentEffectSimple.h"

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
      // MAM.registerPass([&] { return RshCallSiteCounter(); });
      // MAM.registerPass([&] { return RshCallSiteCallerCalleeInfo(); });
      // MAM.registerPass([&] { return RshArgumentTracking(); });
      MAM.registerPass([&] { return RshArgumentEffectSimple(); });
    }

    void runPasses(llvm::Module & m) {
      MPM.run(m, MAM);

      auto argTrackRes = MAM.getResult<RshArgumentEffectSimple>(m);
      for (auto & ele : argTrackRes) {
        auto currFun = ele.first;
        auto currFunData = ele.second;
        std::cout << "        ArgData: " << currFun->getName().str() << std::endl;
        for (auto & data : currFunData) {
          unsigned argIdx = data.first;
          auto calledFuns = data.second; 
          std::cout << "          [" << data.first << "]: ";
          for (auto & funName : calledFuns) {
            std::cout << funName << " ";
          }
          std::cout << std::endl;
        }
      }
      
      // auto argTrackRes = MAM.getResult<RshArgumentTracking>(m);
      // for (auto & ele : argTrackRes) {
      //   auto currFun = ele.first;
      //   auto currFunData = ele.second;
      //   std::cout << "        ArgData: " << currFun->getName().str() << std::endl;
      //   for (auto & data : currFunData) {
      //     std::cout << "          [" << data.first << "]: " << std::endl;
      //     int level = 0;
      //     for (auto & node : data.second) {
      //       std::cout << "             level: " << level++ << ": " << node.getNodeCompressedName() << std::endl;
      //     }
      //   }
      // }
      
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

      // auto callSiteCountRes = MAM.getResult<RshCallSiteCounter>(m);

      // for (auto & ele : callSiteCountRes) {
      //   std::cout << "          " << ele.first().str() << ":" << ele.second << "" << std::endl;
      // }

    }


};

#endif