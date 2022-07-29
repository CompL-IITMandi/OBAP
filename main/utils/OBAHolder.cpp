#include "utils/OBAHolder.h"

#include <ostream>
#include <iostream>
#include <algorithm>
#include "utils/serializerData.h"
#include "utils/TVGraph.h"
#include "utils/UMap.h"
#include "opt/ModuleManager.h"

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_ANALYSIS_RESULTS 0
#define DEBUG_SPACE 6

void OBAHolder::print(const unsigned int & space) {
  printSpace(space);
  std::cout << "=== BitcodeAnalysisResult ===" << std::endl;
  printSpace(space);
  std::cout << "Loaded from  : " << _pathPrefix << std::endl;
  printSpace(space);
  std::cout << "WEIGHT       : " << _analysisWeight << std::endl;

  printSpace(space);
  std::cout << "ARG EFFECT   : " << _analysisArgEffect.size() << " arguments" << std::endl;
  for (auto & ele : _analysisArgEffect) {
    auto argIdx = ele.first;
    auto argEffect = ele.second;
    printSpace(space + 2);
    std::cout << "[Arg " << argIdx << "]" << std::endl;
    for (auto & level : argEffect) {
      printSpace(space + 4);
      std::cout << "|" << level << std::endl;
    }
  }

  printSpace(space);
  std::cout << "FUNCALL BF   : " << _funCallBF.size() << " levels" << std::endl;

  for (unsigned int i = 0; i < _funCallBF.size(); i++) {
    printSpace(space + 2);
    std::cout << "Level " << i << " : [ ";

    for (auto & ele : _funCallBF[i]) {
      std::cout << ele << " ";
    }
    std::cout << "]" << std::endl;
  }

  printSpace(space);
  std::cout << "=============================" << std::endl;
}

void OBAHolder::init() {
  std::stringstream bitcodePath, poolPath;
  bitcodePath << _pathPrefix << ".bc";
  poolPath    << _pathPrefix << ".pool";

  FILE *reader;
  reader = fopen(poolPath.str().c_str(),"r");

  if (!reader) {
    for (int i = 0; i < 10; i++) {
      sleep(1);
      reader = fopen(poolPath.str().c_str(),"r");
      if (reader) break;
    }

    if (!reader) {
      std::cout << "unable to open " << poolPath.str() << std::endl;
      Rf_error("unable to open file!");
      return;
    }
  }

  // Initialize the deserializing stream
  R_inpstream_st inputStream;
  R_InitFileInPStream(&inputStream, reader, R_pstream_binary_format, NULL, R_NilValue);

  SEXP poolDataContainer;
  PROTECT(poolDataContainer = R_Unserialize(&inputStream));

  std::string mainFunName(CHAR(STRING_ELT(VECTOR_ELT(rir::SerializedPool::getFNames(poolDataContainer), 0), 0)));
  _mainFunHandle = mainFunName;

  // load bitcode
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> mb = llvm::MemoryBuffer::getFile(bitcodePath.str().c_str());
  llvm::orc::ThreadSafeContext TSC = std::make_unique<llvm::LLVMContext>();

  // Load the memory buffer into a LLVM module
  llvm::Expected<std::unique_ptr<llvm::Module>> llModuleHolder = llvm::parseBitcodeFile(mb->get()->getMemBufferRef(), *TSC.getContext());

  // Run the passes on the bitcode
  ModuleManager MM(*llModuleHolder.get().get());
  MM.runPasses();

  storeWeightAnalysis(MM);
  storeArgEffectAnalysis(MM);

}

// 1. Weight Analysis Result
void OBAHolder::storeWeightAnalysis(ModuleManager & MM) {
  auto callSiteCounterRes = MM.getRshCallSiteCounterRes();
  unsigned weight = 0;
  for (auto & ele : callSiteCounterRes) {
    weight += ele.second * RshBuiltinWeights::getWeight(ele.first().str());
  }
  _analysisWeight = weight;

  #if DEBUG_ANALYSIS_RESULTS == 1
  printSpace(DEBUG_SPACE);
  std::cout << "== DEBUG (WEIGHT ANALYSIS) ==" << std::endl;

  for (auto & ele : callSiteCounterRes) {
    printSpace(DEBUG_SPACE);
    std::cout << ele.first().str() << " : " << ele.second << "[" << (ele.second * RshBuiltinWeights::getWeight(ele.first().str())) << "]" << std::endl;
  }
  printSpace(DEBUG_SPACE);
  std::cout << "Total : " << weight << std::endl;
  #endif
}

// 2. ArgEffect Analysis Result
void OBAHolder::storeArgEffectAnalysis(ModuleManager & MM) {
  auto argTrackRes = MM.getRshArgumentEffectSimpleRes();

  #if DEBUG_ANALYSIS_RESULTS == 1
  printSpace(DEBUG_SPACE);
  std::cout << "== DEBUG (ARG EFFECT ANALYSIS) ==" << std::endl;

  for (auto & ele : argTrackRes) {
    auto currFun = ele.first;
    auto currFunData = ele.second;

    printSpace(DEBUG_SPACE + 2);
    if (currFun->getName().str() == _mainFunHandle) {
      std::cout << "Function(mainHandle): ";
    } else {
      std::cout << "Function: ";
    }
    std::cout << currFun->getName().str() << std::endl;
    for (auto & data : currFunData) {
      unsigned argIdx = data.first;
      auto calledFuns = data.second;

      printSpace(DEBUG_SPACE + 4);
      std::cout << "[arg" << argIdx << "]: ";
      for (auto & funName : calledFuns) {
        std::cout << funName << " -- ";
      }
      std::cout << std::endl;
    }
  }
  #endif

  for (auto & ele : argTrackRes) {
    auto currFun = ele.first;
    auto currFunData = ele.second;
    if (currFun->getName().str() == _mainFunHandle) {
      _analysisArgEffect = currFunData;
      break;
    }
  }
}

// 3. FunCallBF Analysis Result
void OBAHolder::storeFunCallBFAnalysis(ModuleManager & MM) {
  auto funcCallBFRes = MM.getFunctionCallBreathFirstRes();
  #if DEBUG_ANALYSIS_RESULTS == 1
  printSpace(DEBUG_SPACE);
  std::cout << "== DEBUG (FUN CALL BF) ==" << std::endl;
  for (auto & ele : funcCallBFRes) {
    auto currFunName = ele.first().str();
    auto currFunData = ele.second;

    printSpace(DEBUG_SPACE);
    if (currFunName == _mainFunHandle) {
      std::cout << "Function(mainHandle): ";
    } else {
      std::cout << "Function: ";
    }

    std::cout << currFunName << std::endl;

    unsigned i = 0;
    for (auto & e : currFunData) {
      printSpace(DEBUG_SPACE + 2);
      std::cout << "Level " << ++i << ": " << e.getNodeCompressedName() << std::endl;
    }
  }
  #endif

  for (auto & ele : funcCallBFRes) {
    auto currFunName = ele.first().str();
    auto currFunData = ele.second;
    if (currFunName == _mainFunHandle) {
      std::vector<std::set<std::string>> fc;
      for (auto & e : currFunData) fc.push_back(e.getFunctionSet());
      _funCallBF = fc;
      break;
    }
  }

}