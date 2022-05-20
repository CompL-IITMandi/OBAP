#include "utils/iter.h"

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"
#include "opt/ModuleManager.h"
#include <algorithm>
#include "utils/serializerData.h"

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

void doAnalysisOverContexts(const std::string & pathPrefix, json & contextMap, AnalysisCallback call) {
  std::vector<Context> contextsVec;
  std::unordered_map<Context, unsigned> weightAnalysis;
  std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > simpleArgumentAnalysis;
  std::unordered_map<Context, std::vector<std::set<std::string>>> funCallBFData;

  for (json::iterator currContext = contextMap.begin(); currContext != contextMap.end(); ++currContext) {
    std::string conStr = currContext.value()["context"];
    unsigned long con = std::stoul(conStr);
    Context c(con);

    contextsVec.push_back(c);          
    
    std::stringstream bitcodePath;
    bitcodePath << pathPrefix << conStr << ".bc";

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
      weight += RshBuiltinWeights::getWeight(ele.first().str());
    }
    weightAnalysis[c] = weight;

    std::string mainFunName = currContext.value()["function_names"][0];

    auto argTrackRes = MM.getRshArgumentEffectSimpleRes();
    for (auto & ele : argTrackRes) {
      auto currFun = ele.first;
      auto currFunData = ele.second;
      if (currFun->getName().str() == mainFunName) {
        simpleArgumentAnalysis[c] = currFunData;
        break;
      }
    }

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
    
  }
  call(contextsVec, weightAnalysis, simpleArgumentAnalysis, funCallBFData);

}

void doAnalysisOverContexts(const std::string & pathPrefix, SEXP contextMap, AnalysisCallback call) {
  std::vector<Context> contextsVec;
  std::unordered_map<Context, unsigned> weightAnalysis;
  std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > simpleArgumentAnalysis;
  std::unordered_map<Context, std::vector<std::set<std::string>>> funCallBFData;

  static SEXP maskSym = Rf_install("mask");

  REnvHandler contextMapHandler(contextMap);
  contextMapHandler.iterate([&] (SEXP contextSym, SEXP cData) {
    if (contextSym == maskSym) return;
    contextData cDataHandler(cData);

    unsigned long con = cDataHandler.getContext();
    Context c(con);

    contextsVec.push_back(c);          
      
    std::stringstream bitcodePath;
    bitcodePath << pathPrefix << CHAR(PRINTNAME(contextSym)) << ".bc";

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
      weight += RshBuiltinWeights::getWeight(ele.first().str());
    }
    weightAnalysis[c] = weight;

    std::string mainFunName(CHAR(STRING_ELT(VECTOR_ELT(cDataHandler.getFNames(), 0), 0)));

    auto argTrackRes = MM.getRshArgumentEffectSimpleRes();
    for (auto & ele : argTrackRes) {
      auto currFun = ele.first;
      auto currFunData = ele.second;
      if (currFun->getName().str() == mainFunName) {
        simpleArgumentAnalysis[c] = currFunData;
        break;
      }
    }

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
  });
  call(contextsVec, weightAnalysis, simpleArgumentAnalysis, funCallBFData);
}



void compareContexts(std::vector<Context> & contextsVec, ComparisonCallback call) {
  static unsigned comparisonLevel = getenv("CLEVEL") ? std::stoi(getenv("CLEVEL")) : 4;

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
      // Roughly comparable
      else if (other.roughlySmaller(currCon) && comparisonLevel > 0) {
        call(currCon, other, ComparisonType::ROUGH);
      } else if (currCon.roughlySmaller(other) && comparisonLevel > 0) {
        call(other, currCon, ComparisonType::ROUGH);
      }
      // Num Missing == 0
      else if (currCon.missing == 0 && other.missing == 0 && comparisonLevel > 1) {
        call(other, currCon, ComparisonType::DIFFZEROMISS);
      }
      // Num Missing a == Num Missing b
      else if (currCon.missing == other.missing && comparisonLevel > 2) {
        call(other, currCon, ComparisonType::DIFFSAMEMISS);
      }
      // Num Missing a != Num Missing b
      else if (currCon.missing != other.missing && comparisonLevel > 3) {
        call(other, currCon, ComparisonType::DIFFDIFFMISS);
      }



      it_other++;
    }
  }
}


std::string GlobalData::bitcodesFolder = "";