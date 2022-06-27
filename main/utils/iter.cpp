#include "utils/iter.h"

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"
#include "opt/ModuleManager.h"
#include <algorithm>
#include "utils/serializerData.h"

#define DEBUG_ANALYSIS 0

void iterateOverBitcodes(json & processedJson, IterCallback call) {
  for (json::iterator it_meta = processedJson.begin(); it_meta != processedJson.end(); ++it_meta) {
    auto meta = it_meta.key();
    auto currVal = it_meta.value();
    auto hast = currVal["hast"];
    auto name = currVal["name"];
    auto offsetMap = currVal["offsetMap"];

    for (json::iterator it_offset = offsetMap.begin(); it_offset != offsetMap.end(); ++it_offset) {
      auto offset = it_offset.key();
      auto contextMap = it_offset.value();
      call(
        meta,
        hast,
        name,
        offset,
        contextMap
      );
    }
  }
}

// void doAnalysisOverContexts(const std::string & pathPrefix, json & contextMap, AnalysisCallback call) {
//   std::vector<Context> contextsVec;
//   std::unordered_map<Context, unsigned> weightAnalysis;
//   std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > simpleArgumentAnalysis;
//   std::unordered_map<Context, std::vector<std::set<std::string>>> funCallBFData;

//   for (json::iterator currContext = contextMap.begin(); currContext != contextMap.end(); ++currContext) {
//     std::string conStr = currContext.value()["context"];
//     unsigned long con = std::stoul(conStr);
//     Context c(con);

//     contextsVec.push_back(c);          
    
//     std::stringstream bitcodePath;
//     bitcodePath << pathPrefix << conStr << ".bc";

//     // load bitcode
//     llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> mb = llvm::MemoryBuffer::getFile(bitcodePath.str().c_str());

//     llvm::orc::ThreadSafeContext TSC = std::make_unique<llvm::LLVMContext>();

//     // Load the memory buffer into a LLVM module
//     llvm::Expected<std::unique_ptr<llvm::Module>> llModuleHolder = llvm::parseBitcodeFile(mb->get()->getMemBufferRef(), *TSC.getContext());

//     // ModuleManager to run our passes on the bitcodes
//     ModuleManager MM(*llModuleHolder.get().get());

//     MM.runPasses();

//     auto callSiteCounterRes = MM.getRshCallSiteCounterRes();

//     unsigned weight = 0;

//     for (auto & ele : callSiteCounterRes) {
//       weight += RshBuiltinWeights::getWeight(ele.first().str());
//     }
//     weightAnalysis[c] = weight;

//     std::string mainFunName = currContext.value()["function_names"][0];

//     auto argTrackRes = MM.getRshArgumentEffectSimpleRes();
//     for (auto & ele : argTrackRes) {
//       auto currFun = ele.first;
//       auto currFunData = ele.second;
//       if (currFun->getName().str() == mainFunName) {
//         simpleArgumentAnalysis[c] = currFunData;
//         break;
//       }
//     }

//     auto funcCallBFRes = MM.getFunctionCallBreathFirstRes();
//     for (auto & ele : funcCallBFRes) {
//       auto currFunName = ele.first().str();
//       auto currFunData = ele.second;
//       if (currFunName == mainFunName) {
//         std::vector<std::set<std::string>> fc;
//         for (auto & e : currFunData) fc.push_back(e.getFunctionSet());
//         funCallBFData[c] = fc;
//         break;
//       }
//     }
    
//   }
//   call(contextsVec, weightAnalysis, simpleArgumentAnalysis, funCallBFData);

// }

void doAnalysisOverContexts(const std::string & pathPrefix, SEXP contextMap, AnalysisCallback call) {
  std::vector<Context> contextsVec;
  std::unordered_map<Context, unsigned> weightAnalysis;
  std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > simpleArgumentAnalysis;
  std::unordered_map<Context, std::vector<std::set<std::string>>> funCallBFData;

  #if DEBUG_ANALYSIS == 1
  std::cout << "Starting analysis over: " << pathPrefix << std::endl;
  #endif

  static SEXP maskSym = Rf_install("mask");

  REnvHandler contextMapHandler(contextMap);
  contextMapHandler.iterate([&] (SEXP contextSym, SEXP cData) {
    if (contextSym == maskSym) return;
    contextData cDataHandler(cData);

    unsigned long con = cDataHandler.getContext();
    Context c(con);

    #if DEBUG_ANALYSIS == 1
    std::cout << "  context: (" << con << ")" << c << std::endl;
    #endif

    contextsVec.push_back(c);          
      
    std::stringstream bitcodePath;
    bitcodePath << pathPrefix << CHAR(PRINTNAME(contextSym)) << ".bc";

    #if DEBUG_ANALYSIS == 1
    std::cout << "    Analyzing: " << bitcodePath.str() << std::endl;
    #endif

    // load bitcode
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> mb = llvm::MemoryBuffer::getFile(bitcodePath.str().c_str());
    llvm::orc::ThreadSafeContext TSC = std::make_unique<llvm::LLVMContext>();

    // Load the memory buffer into a LLVM module
    llvm::Expected<std::unique_ptr<llvm::Module>> llModuleHolder = llvm::parseBitcodeFile(mb->get()->getMemBufferRef(), *TSC.getContext());

    // ModuleManager to run our passes on the bitcodes
    ModuleManager MM(*llModuleHolder.get().get());

    MM.runPasses();

    auto callSiteCounterRes = MM.getRshCallSiteCounterRes();

    unsigned weight = 0;
    for (auto & ele : callSiteCounterRes) {
      weight += ele.second * RshBuiltinWeights::getWeight(ele.first().str());
    }
    
    #if DEBUG_ANALYSIS == 1
    std::cout << "    == CALL SITE OPCODE SIMILARITY ==" << std::endl;
    for (auto & ele : callSiteCounterRes) {
      std::cout << "      " << ele.first().str() << " : " << ele.second << "[" << (ele.second * RshBuiltinWeights::getWeight(ele.first().str())) << "]" << std::endl;
    }
    std::cout << "        " << "Total : " << weight << std::endl;
    #endif

    weightAnalysis[c] = weight;

    std::string mainFunName(CHAR(STRING_ELT(VECTOR_ELT(cDataHandler.getFNames(), 0), 0)));

    // std::cout << c.toI() << " --> " << mainFunName << std::endl;


    auto argTrackRes = MM.getRshArgumentEffectSimpleRes();
    for (auto & ele : argTrackRes) {
      auto currFun = ele.first;
      auto currFunData = ele.second;
      if (currFun->getName().str() == mainFunName) {
        simpleArgumentAnalysis[c] = currFunData;
        break;
      }
    }

    #if DEBUG_ANALYSIS == 1
    std::cout << "    == ARG EFFECT TRACKING ==" << std::endl;
    for (auto & ele : argTrackRes) {
      auto currFun = ele.first;
      auto currFunData = ele.second;
      std::cout << "      Function: " << currFun->getName().str() << std::endl;
      for (auto & data : currFunData) {
        unsigned argIdx = data.first;
        auto calledFuns = data.second; 
        std::cout << "        [arg" << argIdx << "]: ";
        for (auto & funName : calledFuns) {
          std::cout << funName << " -- ";
        }
        std::cout << std::endl;
      }
    }
    #endif

    auto funcCallBFRes = MM.getFunctionCallBreathFirstRes();
    for (auto & ele : funcCallBFRes) {
      auto currFunName = ele.first().str();
      auto currFunData = ele.second;
      if (currFunName == mainFunName) {
        std::vector<std::set<std::string>> fc;
        for (auto & e : currFunData) fc.push_back(e.getFunctionSet());
        funCallBFData[c] = fc;
        break;
      }
    }

    #if DEBUG_ANALYSIS == 1
    std::cout << "    == FUN CALL BF ==" << std::endl;
    for (auto & ele : funcCallBFRes) {
      auto currFunName = ele.first().str();
      auto currFunData = ele.second;
      std::cout << "      " << currFunName;
      if (currFunName == mainFunName) {
        std::cout << " [MAIN]";
      }
      std::cout << std::endl;
      unsigned i = 0;
      for (auto & e : currFunData) {
        std::cout << "        " << ++i << ": " << e.getNodeCompressedName() << std::endl;
      }
    }

    #endif


  });
  call(contextsVec, weightAnalysis, simpleArgumentAnalysis, funCallBFData);
}



void compareContexts(std::vector<Context> & contextsVec, ComparisonCallback call) {
  for (auto it_currCon = contextsVec.begin(); it_currCon != contextsVec.end(); ++it_currCon) {
    auto it_other = it_currCon + 1;
    while (it_other != contextsVec.end()) {
      auto currCon = *it_currCon;
      auto other = *it_other;
      
      // Strictly comparable
      if (other.smaller(currCon)) {
        call(currCon, other, ComparisonType::STRICT);
      } else if (currCon.smaller(other)) {
        call(other, currCon, ComparisonType::STRICT);
      }
      // Roughly comparable - EQ
      else if (other.roughlySmaller(currCon) && currCon.roughlySmaller(other)) {
        call(currCon, other, ComparisonType::ROUGH_EQ);
      }

      // Roughlt comparable - NEQ
      else if (other.roughlySmaller(currCon)) {
        call(currCon, other, ComparisonType::ROUGH_NEQ);
      } else if (currCon.roughlySmaller(other)) {
        call(other, currCon, ComparisonType::ROUGH_NEQ);
      }      
      it_other++;
    }
  }
}


std::string GlobalData::bitcodesFolder = "";