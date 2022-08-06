#include "utils/OBAHolder.h"

#include <ostream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include "utils/serializerData.h"
#include "utils/TVGraph.h"
#include "utils/UMap.h"
#include "opt/ModuleManager.h"

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_ANALYSIS_RESULTS 0
#define DEBUG_SPACE 8
#define DEBUG_ARG_EFFECT_ANALYSIS 0
#define DEBUG_FUNCALL_COMPARISON 0
#define DEBUG_SIMILARITY_CHECK 0

void OBAHolder::print(const unsigned int & space) const {
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

rir::FunctionSignature OBAHolder::getFS() {
  return _functionSignature;
}

static std::set<std::string> getReqMapAsCppSet(SEXP rData) {
  std::set<std::string> reqSet;

  for (int i = 0; i < Rf_length(rData); i++) {
    SEXP ele = VECTOR_ELT(rData, i);
    reqSet.insert(CHAR(PRINTNAME(ele)));
  }

  return reqSet;
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

  reqMap = getReqMapAsCppSet(rir::contextData::getReqMapAsVector(_cData));

  unsigned int protecc = 0;
  // rir::SerializedPool::recursivelyProtect(poolDataContainer);  


  fclose(reader);

  _functionSignature = rir::SerializedPool::getFS(poolDataContainer);


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
  storeFunCallBFAnalysis(MM);

  UNPROTECT(protecc + 1);

  

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
    printSpace(DEBUG_SPACE+2);
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
        std::cout << "|" << funName << " -- ";
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

    printSpace(DEBUG_SPACE + 2);
    if (currFunName == _mainFunHandle) {
      std::cout << "Function(mainHandle): ";
    } else {
      std::cout << "Function: ";
    }

    std::cout << currFunName << std::endl;

    unsigned i = 0;
    for (auto & e : currFunData) {
      printSpace(DEBUG_SPACE + 4);
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

static std::pair<unsigned, std::vector<unsigned>> checkArgEffectSimilarity(std::vector<std::pair<unsigned, std::vector<std::string>>> a1, std::vector<std::pair<unsigned, std::vector<std::string>>> a2);

static int checkFunCallSimilarity(const std::vector<std::set<std::string>> & currV, const std::vector<std::set<std::string>> & otherV);

// Equality Check
ComparisonResult OBAHolder::equals(const OBAHolder& other) {
  // 
  // 1. Weight analysis similarity
  // 2. Argeffect similarity
  // 3. Funcall similarity
  // 
  
  bool weightSimilarity;
  int funcallSimilarity;
  int weightDiff = this->_analysisWeight - other._analysisWeight;
  if (weightDiff < 0) weightDiff = -weightDiff;
  weightSimilarity = weightDiff <= THRESHOLD_WEIGHT;

  funcallSimilarity = checkFunCallSimilarity(this->_funCallBF, other._funCallBF);

  auto argEffectRes = checkArgEffectSimilarity(this->_analysisArgEffect, other._analysisArgEffect);

  bool result = weightSimilarity && (funcallSimilarity == 0);

  #if DEBUG_SIMILARITY_CHECK > 0
  std::cout << "=== COMPARING BINS ===" << std::endl;
  this->print(DEBUG_SPACE);
  other.print(DEBUG_SPACE);
  
  printSpace(DEBUG_SPACE);
  std::cout << "Weight Similarity: " << weightSimilarity << (weightSimilarity ? " (SIMILAR)" : "(NOT SIMILAR)") << std::endl;
  printSpace(DEBUG_SPACE);
  std::cout << "Fun Call Similarity: " << funcallSimilarity << (funcallSimilarity == 0 ? " (SIMILAR)" : "(NOT SIMILAR)") << std::endl;

  printSpace(DEBUG_SPACE);
  std::cout << "Arg Effect Res(" << argEffectRes.first << "): [ ";
  for (auto & ele : argEffectRes.second) {
    std::cout << ele << " ";
  }
  std::cout << "]" << std::endl;
  

  #endif

  return ComparisonResult(result, argEffectRes.first, argEffectRes.second);
}

unsigned OBAHolder::THRESHOLD_WEIGHT    = getenv("THRESHOLD_WEIGHT")    ? std::stoi(getenv("THRESHOLD_WEIGHT"))    : 0;
unsigned OBAHolder::THRESHOLD_FUNCALL   = getenv("THRESHOLD_FUNCALL")   ? std::stoi(getenv("THRESHOLD_FUNCALL"))   : 0;
unsigned OBAHolder::THRESHOLD_ARGEFFECT = getenv("THRESHOLD_ARGEFFECT") ? std::stoi(getenv("THRESHOLD_ARGEFFECT")) : 0;


static void tokenize(std::set<std::string> & res, std::string s, std::string del = " ") {
  int start = 0;
  int end = s.find(del);
  while (end != -1) {
    res.insert(s.substr(start, end - start));
    start = end + del.size();
    end = s.find(del, start);
  }
  res.insert(s.substr(start, end - start));
}

// 0 -> [Non conclusive] There were no comparable indices
// 1 -> [All comparable were similar] The number of comparable indices were > 0 but the arglist size was different
// 2 -> [All comparable were similar] The number of comparable indices were > 0 and arglist were identical
// 3 -> [Not similar] If any comparable arg was different, then this is returned
// 4 -> [Not similar] If unexpected error occurs
static std::pair<unsigned, std::vector<unsigned>> checkArgEffectSimilarity(std::vector<std::pair<unsigned, std::vector<std::string>>> a1, std::vector<std::pair<unsigned, std::vector<std::string>>> a2) {
  std::unordered_map<unsigned, std::vector<std::string>> a1Res, a2Res;
  std::unordered_map<unsigned, std::vector<std::set<std::string>>> a1ArgSet, a2ArgSet;
  std::vector<unsigned> argList1, argList2, comparable;

  std::vector<unsigned> similarIdx;

  // Prepare for comparison
  for (auto & ele : a1) {
    auto argIdx = ele.first;
    if (a1Res.find(argIdx) != a1Res.end()) {
      std::cout << "[WARN]: unexpected duplicate argeffect result found in current" << std::endl;
      return std::pair<unsigned, std::vector<unsigned>>(4, similarIdx);
    } else {
      a1Res[argIdx] = ele.second;
      argList1.push_back(argIdx);
    }
  }

  for (auto & ele : a2) {
    auto argIdx = ele.first;
    if (a2Res.find(argIdx) != a2Res.end()) {
      std::cout << "[WARN]: unexpected duplicate argeffect result found in other" << std::endl;
      return std::pair<unsigned, std::vector<unsigned>>(4, similarIdx);
    } else {
      a2Res[argIdx] = ele.second;
      argList2.push_back(argIdx);
    }
  }

  std::sort(argList1.begin(), argList1.end());
  std::sort(argList2.begin(), argList2.end());
  
  std::set_intersection(argList1.begin(), argList1.end(),
                        argList2.begin(), argList2.end(),
                        std::back_inserter(comparable));
  
  if (comparable.size() == 0) {
    return std::pair<unsigned, std::vector<unsigned>>(0, similarIdx);
  }

  for (auto & ele : a1Res) {
    std::vector<std::set<std::string>> v;
    for (auto & e : ele.second) {
      std::set<std::string> rrr;
      tokenize(rrr, e, "|");
      v.push_back(rrr);
    }
    a1ArgSet[ele.first] = v;
  }

  for (auto & ele : a2Res) {
    std::vector<std::set<std::string>> v;
    for (auto & e : ele.second) {
      std::set<std::string> rrr;
      tokenize(rrr, e, "|");
      v.push_back(rrr);
    }
    a2ArgSet[ele.first] = v;
  }

  // Compare comparable arguments

  bool allSimilar = true;

  for (auto & idx : comparable) {
    auto c1LevelData = a1ArgSet[idx];
    auto c2LevelData = a2ArgSet[idx];

    if (c1LevelData.size() == c2LevelData.size()) {
      for (unsigned i = 0; i < c1LevelData.size(); i++) {
        
        std::set<std::string> v1(c1LevelData[i].begin(), c1LevelData[i].end());
        std::set<std::string> v2(c2LevelData[i].begin(), c2LevelData[i].end());

        #if DEBUG_ARG_EFFECT_ANALYSIS == 1
        std::cout << "  Level " << i << std::endl;
        std::cout << "    v1: [";
        for (auto & e : v1) {
          std::cout << e << " ";
        }
        std::cout << "]" << std::endl;

        std::cout << "    v2: [";
        for (auto & e : v2) {
          std::cout << e << " ";
        }
        std::cout << "]" << std::endl;
        #endif

        std::vector<std::string> diff1;
        //no need to sort since it's already sorted
        std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
          std::inserter(diff1, diff1.begin()));

        std::vector<std::string> diff2;
        //no need to sort since it's already sorted
        std::set_difference(v2.begin(), v2.end(), v1.begin(), v1.end(),
          std::inserter(diff2, diff2.begin()));

        
        #if DEBUG_ARG_EFFECT_ANALYSIS == 1
        std::cout << "    diff: [";
        for (auto & ele : diff1) {
          std::cout << ele << " ";
        }
        for (auto & ele : diff2) {
          std::cout << ele << " ";
        }
        std::cout << "]" << std::endl;
        #endif

        if (diff1.size() == 0 && diff2.size() == 0) {
          // success = true;
        } else {
          allSimilar = false;
        }
      }
    } else {
      allSimilar = false;
    }

    if (!allSimilar) {
      break;
    } else {
      similarIdx.push_back(idx);
    }
  }

  if (allSimilar) {
    if (argList1.size() == argList2.size() && argList1.size() == comparable.size()) {
      return std::pair<unsigned, std::vector<unsigned>>(2, comparable);
    } else {
      return std::pair<unsigned, std::vector<unsigned>>(1, comparable);
    }
  } else {
    return std::pair<unsigned, std::vector<unsigned>>(0, similarIdx);
  }
}


// -1 -> [Not similar] Number of levels are different
// 0  -> [Similar] Number of levels are 0 or difference is 0
// >0 -> [Not similar] Number of non-similar levels
static int checkFunCallSimilarity(const std::vector<std::set<std::string>> & currV, const std::vector<std::set<std::string>> & otherV) {
  auto levelsInCurrent = currV.size();
  auto levelsInOther = otherV.size();
  unsigned callOrderDifference = 0;
  
  #if DEBUG_FUNCALL_COMPARISON > 0
  printSpace(DEBUG_SPACE);
  std::cout << "=== FUN CALL SIMILARITY ===" << std::endl;
  printSpace(DEBUG_SPACE);
  std::cout << "currV(" << currV.size() << ")" << std::endl;

  #if DEBUG_FUNCALL_COMPARISON > 1
  for (unsigned int i = 0; i < currV.size(); i++) {
    printSpace(DEBUG_SPACE+2);
    std::cout << "Level " << i << " : [ ";
    for (auto & ele : currV[i]) {
      std::cout << ele << " ";
    }
    std::cout << "]" << std::endl;
  }
  #endif

  printSpace(DEBUG_SPACE);
  std::cout << "otherV(" << otherV.size() << ")" << std::endl;

  #if DEBUG_FUNCALL_COMPARISON > 1
  for (unsigned int i = 0; i < otherV.size(); i++) {
    printSpace(DEBUG_SPACE+2);
    std::cout << "Level " << i << " : [ ";
    for (auto & ele : otherV[i]) {
      std::cout << ele << " ";
    }
    std::cout << "]" << std::endl;
  }
  #endif

  #endif
  
  if (levelsInCurrent == 0 && levelsInOther == 0) {
    return 0;
  }

  if (levelsInCurrent != levelsInOther) return -1;

  for (unsigned i = 0; i < levelsInCurrent; i++) {
    auto v1 = currV[i];
    auto v2 = otherV[i];
    std::vector<std::string> diff1, diff2;
    //no need to sort since it's already sorted
    std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
      std::inserter(diff1, diff1.begin()));
    std::set_difference(v2.begin(), v2.end(), v1.begin(), v1.end(),
      std::inserter(diff2, diff2.begin()));

    #if DEBUG_FUNCALL_COMPARISON > 1
    printSpace(DEBUG_SPACE);
    std::cout << "Diff at Level " << i << " (" << (diff1.size() + diff2.size()) << "): [ ";
    for (auto & ele : diff1) {
      std::cout << ele << " ";
    }
    for (auto & ele : diff2) {
      std::cout << ele << " ";
    }
    std::cout << "]" << std::endl;
    #endif

    if (diff1.size() > 0 || diff2.size() > 0) {
      callOrderDifference++;
    }
  }

  #if DEBUG_FUNCALL_COMPARISON > 0
  printSpace(DEBUG_SPACE);
  std::cout << "Diff: " << callOrderDifference << std::endl;
  #endif

  return callOrderDifference;
}