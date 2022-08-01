#include "utils/SerializedDataProcessor.h"

#include <ostream>
#include <iostream>
#include <algorithm>
#include "utils/serializerData.h"
#include "utils/TVGraph.h"
#include "utils/UMap.h"
#include "opt/ModuleManager.h"
#include "utils/OBAHolder.h"

#define DEBUG_LOCALS_INIT 0

// Possible values 0, 1, 2
#define DEBUG_CONTEXTWISE_SIMILARITY_CHECK 0
#define DEBUG_CONTEXTWISE_TYPE_VERSIONING 0


#define PRINT_REPRESENTATIVE_SELECTION_NON_TRIVIAL_CASES 0

// 
// Takes a vector of similar binaries and returns the one with the smallest requirement map
//  If they are identical, then the one with smallest requirement map is going to link the fastest so we choose that.
//    * In most cases requirement maps are going to be identical, so it does not matter which one we choose
// 
static std::pair<SEXP, SEXP> chooseRepresentative(std::vector<std::pair<SEXP, SEXP>> possibilities) {
  std::pair<SEXP, SEXP> representative;

  int currMin = -1;
  int currMax = INT_MAX;

  for (unsigned int i = 0; i < possibilities.size(); i++) {
    SEXP cData = possibilities[i].second;
    SEXP rData = rir::contextData::getReqMapAsVector(cData);
    int size = Rf_length(rData);

    // First case, C++ pleae optimize this, unroll and magic!
    if (currMin == -1) {
      representative.first = possibilities[i].first;
      representative.second = possibilities[i].second;
      currMin = size;
      currMax = size;
      continue;
    }

    // Update current min
    if (size < currMin) {
      representative.first = possibilities[i].first;
      representative.second = possibilities[i].second;
      currMin = size;
    }

    // Update current max
    if (size > currMax) {
      currMax = size;
    }
  }
  
  #if PRINT_REPRESENTATIVE_SELECTION_NON_TRIVIAL_CASES == 1
  if (currMin != currMax) {
    printSpace(4);
    std::cout << "Non Trivial represnentative selection case" << std::endl;
    for (int i = 0; i < possibilities.size(); i++) {
      SEXP cData = possibilities[i].second;
      rir::contextData::print(cData, 6);
    }
  }
  #endif



  return representative;

}


void SerializedDataProcessor::init() {
  static SEXP maskSym = Rf_install("mask");

  // 
  // 1. Initialize locals
  // 
  REnvHandler contextMapHandler(_cData);
  contextMapHandler.iterate([&] (SEXP epochSym, SEXP cData) {
    if (epochSym == maskSym) return;

    #if DEBUG_LOCALS_INIT == 1
    rir::contextData::print(cData, 4);
    #endif

    // Count the number of binaries found
    _origBitcodes++;

    // Add to original bitcode list
    unsigned long con = rir::contextData::getContext(cData);
    _origContextWiseData[con].push_back(std::pair<SEXP, SEXP>(epochSym, cData));
  });

  // 
  // 2. Contextwise binary reduction
  // 
  #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 0
  printSpace(6);
  std::cout << "=== CONTEXTWISE SIMILARITY CHECK ===" << std::endl;
  #endif
  for (auto & ele : _origContextWiseData) {
    unsigned long con = ele.first;
    std::vector<std::pair<SEXP, SEXP>> cDataVec = ele.second;

    #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 0
    printSpace(8);
    std::cout << "Context(" << con << "): " << cDataVec.size() << " binaries" << std::endl;
    #endif

    // Prefix  : HAST_OFFSET_EPOCH
    // Bitcode : Prefix.bc
    // Pool    : Prefix.pool

    std::vector<unsigned int> removed;
    std::vector<std::pair<SEXP, SEXP>> groups;
    for (unsigned int i = 0; i < cDataVec.size(); i++) {
      if (std::find(removed.begin(), removed.end(), i) != removed.end()) {
        continue;
      }

      std::vector<std::pair<SEXP, SEXP>> similars;
      similars.push_back(cDataVec[i]);
      
      std::stringstream pref1;
      pref1 << _pathPrefix << CHAR(PRINTNAME(cDataVec[i].first));
      OBAHolder r1(pref1.str());

      #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 1
      printSpace(8);
      std::cout << "(" << i << ")" << std::endl;
      r1.print(8);
      #endif

      for (unsigned int j = i + 1; j < cDataVec.size(); j++) {
        if (std::find(removed.begin(), removed.end(), j) != removed.end()) {
          continue;
        }
        std::stringstream pref2;
        pref2 << _pathPrefix << CHAR(PRINTNAME(cDataVec[j].first));
        OBAHolder r2(pref2.str());

        #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 1
        printSpace(10);
        std::cout << "(" << i << "," << j << ")" << std::endl;
        r2.print(10);
        #endif

        auto cRes = r1.equals(r2);
        bool argSimilar = cRes.argEffectResult <= 2;
        
        #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 1
        printSpace(8);
        std::cout << "(" << i << "," << j << ")";
        #endif
        if (cRes.similar && argSimilar) {
          removed.push_back(j);
          _deprecatedBitcodes++;

          similars.push_back(cDataVec[j]);

          #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 1
          std::cout << " [SIMILAR]";
          #endif
        }

        #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 1
        std::cout << std::endl;
        #endif
      }

      auto representative = chooseRepresentative(similars);

      _reducedContextWiseData[con].push_back(representative);
    }
  }

  // 
  // 3. Type versioning
  //

  #if DEBUG_CONTEXTWISE_TYPE_VERSIONING > 0
  printSpace(6);
  std::cout << "=== CONTEXTWISE TYPE VERSIONING ===" << std::endl;
  #endif
  for (auto & ele : _reducedContextWiseData) {
    #if DEBUG_CONTEXTWISE_TYPE_VERSIONING > 0
    printSpace(8);
    std::cout << "Processing context(" << ele.first << "): " << ele.second.size() << std::endl;
    #endif
    if (ele.second.size() > 1) {
      #if DEBUG_CONTEXTWISE_TYPE_VERSIONING > 0
      printSpace(8);
      std::cout << "(*) TV needed" << std::endl;
      #endif

      TVGraph g(ele.second);
      auto stat = g.init();
      if (!stat) {
        Rf_error("Init failed for TVGraph");
      }

      _tvGraphData[ele.first] = g;

      if (g.getNumTypeVersions() > 1) {
        #if DEBUG_CONTEXTWISE_TYPE_VERSIONING > 0
        printSpace(8);
        std::cout << "(*) TV good case: " << g.getNumTypeVersions() << " TVs" << std::endl;
        g.print(10);
        #endif
      } else {
        #if DEBUG_CONTEXTWISE_TYPE_VERSIONING > 0
        printSpace(8);
        std::cout << "(*) TV bad case" << std::endl;
        
        g.print(10);
        #endif
      }
    } else {
      #if DEBUG_CONTEXTWISE_TYPE_VERSIONING > 0
      printSpace(8);
      std::cout << "(*) TV not needed" << std::endl;
      #endif
    }
    
    
  }

}


void SerializedDataProcessor::print(const unsigned int & space) {
  printSpace(space);
  std::cout << "=== SerializedDataProcessor ===" << std::endl;

  printSpace(space);
  std::cout << "Filename                : " << _pathPrefix << std::endl;


  if (_origContextWiseData.size() > 0) {
    printSpace(space);
    std::cout << "Discovered " << _origContextWiseData.size() << " contexts" << std::endl;
  }

  for (auto & ele : _origContextWiseData) {
    printSpace(space + 2);
    std::cout << "├─(" << ele.first << "): " << ele.second.size() << " binaries" << std::endl;
  }

  // bool typeVersioningNeeded = false;

  if (_reducedContextWiseData.size() > 0) {
    printSpace(space);
    std::cout << "Reduced " << _reducedContextWiseData.size() << " contexts" << std::endl;
  }

  unsigned int finalBins = 0;

  for (auto & ele : _reducedContextWiseData) {
    printSpace(space + 2);
    if (_tvGraphData.find(ele.first) != _tvGraphData.end()) {
      finalBins += _tvGraphData[ele.first].getBinariesCount();

      std::cout << "├─(" << ele.first << "): " << _tvGraphData[ele.first].getBinariesCount() << " binaries" << std::endl;
      _tvGraphData[ele.first].print(space + 4);
    } else {
      finalBins += ele.second.size() + 1;
      std::cout << "├─(" << ele.first << "): " << ele.second.size() << " binaries" << std::endl;
    }
    // if (ele.second.size() > 1) {
    //   typeVersioningNeeded = true;
    // }
  }

  // printSpace(space);
  // if (typeVersioningNeeded) {
  //   std::cout << "(*) Type Versioning Needed" << std::endl;
  // } else {
  //   std::cout << "(*) Type Versioning Not Needed" << std::endl;
  // }

  printSpace(space);
  std::cout << "Original Contexts count : " << _origContextWiseData.size() << std::endl;

  printSpace(space);
  std::cout << "Final Contexts count    : " << (_origContextWiseData.size() - _deprecatedContexts) << std::endl;

  printSpace(space);
  std::cout << "Original Bitcodes count : " << _origBitcodes << std::endl;

  // printSpace(space);
  // std::cout << "Deprecated Bitcodes count    : " << _deprecatedBitcodes << std::endl;

  printSpace(space);
  std::cout << "Final Bitcodes count    : " << finalBins << " (" << _deprecatedBitcodes << " directly deprecated)" << std::endl;

  printSpace(space);
  std::cout << "===============================" << std::endl;
}

// void doAnalysisOverContexts(const std::string & pathPrefix, SEXP contextMap, AnalysisCallback call) {
//   static SEXP maskSym = Rf_install("mask");
//   std::unordered_map<unsigned long, std::vector<SEXP>> contexts;

//   std::cout << "Processing: " << pathPrefix << std::endl;

//   REnvHandler contextMapHandler(contextMap);
//   contextMapHandler.iterate([&] (SEXP epochSym, SEXP cData) {
//     if (epochSym == maskSym) return;
//     // #if DEBUG_CHECKPOINTS == 1
//     // std::cout << "EPOCH: " << CHAR(PRINTNAME(epochSym)) << std::endl;
//     // rir::contextData::print(cData, 2);
//     // #endif

//     unsigned long con = rir::contextData::getContext(cData);

//     contexts[con].push_back(cData);
//   });


//   for (auto & ele : contexts) {
//     std::cout << "  Processing context: " << rir::Context(ele.first) << std::endl;
//     TVGraph g(ele.second);
//     auto stat = g.init();
//     g.print();

//     if (!stat) {
//       Rf_error("Init failed for TVGraph");
//     }
//   }

//   // std::vector<rir::Context> contextsVec;
//   // std::unordered_map<rir::Context, unsigned> weightAnalysis;
//   // std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > simpleArgumentAnalysis;
//   // std::unordered_map<rir::Context, std::vector<std::set<std::string>>> funCallBFData;

//   // #if DEBUG_CHECKPOINTS == 1
//   // std::cout << "Starting analysis over: " << pathPrefix << std::endl;
//   // #endif


//   // REnvHandler contextMapHandler(contextMap);
//   // contextMapHandler.iterate([&] (SEXP contextSym, SEXP cData) {
//   //   if (contextSym == maskSym) return;

//   //   unsigned long con = rir::contextData::getContext(cData);
//   //   rir::Context c(con);

    

//   //   contextsVec.push_back(c);          
      
//   //   std::stringstream bitcodePath, poolPath;
//   //   bitcodePath << pathPrefix << CHAR(PRINTNAME(contextSym)) << ".bc";
//   //   poolPath << pathPrefix << CHAR(PRINTNAME(contextSym)) << ".pool";

//   //   #if DEBUG_CHECKPOINTS == 1
//   //   std::cout << "    Analyzing: " << bitcodePath.str() << std::endl;
//   //   #endif

//   //   FILE *reader;
//   //   reader = fopen(poolPath.str().c_str(),"r");

//   //   if (!reader) {
//   //     for (int i = 0; i < 10; i++) {
//   //       sleep(1);
//   //       // std::cout << "waiting to open: " << metadataPath.str() << std::endl;
//   //       reader = fopen(poolPath.str().c_str(),"r");
//   //       if (reader) break;
//   //     }

//   //     if (!reader) {
//   //       std::cout << "unable to open " << poolPath.str() << std::endl;
//   //       Rf_error("unable to open file!");
//   //       return;
//   //     }
//   //   }

//   //   // Initialize the deserializing stream
//   //   R_inpstream_st inputStream;
//   //   R_InitFileInPStream(&inputStream, reader, R_pstream_binary_format, NULL, R_NilValue);

//   //   SEXP poolDataContainer;
//   //   PROTECT(poolDataContainer = R_Unserialize(&inputStream));

//   //   // load bitcode
//   //   llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> mb = llvm::MemoryBuffer::getFile(bitcodePath.str().c_str());
//   //   llvm::orc::ThreadSafeContext TSC = std::make_unique<llvm::LLVMContext>();

//   //   // Load the memory buffer into a LLVM module
//   //   llvm::Expected<std::unique_ptr<llvm::Module>> llModuleHolder = llvm::parseBitcodeFile(mb->get()->getMemBufferRef(), *TSC.getContext());

//   //   // ModuleManager to run our passes on the bitcodes
//   //   ModuleManager MM(*llModuleHolder.get().get());

//   //   MM.runPasses();

//   //   auto callSiteCounterRes = MM.getRshCallSiteCounterRes();

//   //   unsigned weight = 0;
//   //   for (auto & ele : callSiteCounterRes) {
//   //     weight += ele.second * RshBuiltinWeights::getWeight(ele.first().str());
//   //   }
    
//   //   #if DEBUG_CHECKPOINTS == 1
//   //   std::cout << "    == CALL SITE OPCODE SIMILARITY ==" << std::endl;
//   //   for (auto & ele : callSiteCounterRes) {
//   //     std::cout << "      " << ele.first().str() << " : " << ele.second << "[" << (ele.second * RshBuiltinWeights::getWeight(ele.first().str())) << "]" << std::endl;
//   //   }
//   //   std::cout << "        " << "Total : " << weight << std::endl;
//   //   #endif

//   //   weightAnalysis[c] = weight;

//   //   std::string mainFunName(CHAR(STRING_ELT(VECTOR_ELT(rir::SerializedPool::getFNames(poolDataContainer), 0), 0)));

//   //   // std::cout << c.toI() << " --> " << mainFunName << std::endl;


//   //   auto argTrackRes = MM.getRshArgumentEffectSimpleRes();
//   //   for (auto & ele : argTrackRes) {
//   //     auto currFun = ele.first;
//   //     auto currFunData = ele.second;
//   //     if (currFun->getName().str() == mainFunName) {
//   //       simpleArgumentAnalysis[c] = currFunData;
//   //       break;
//   //     }
//   //   }

//   //   #if DEBUG_CHECKPOINTS == 1
//   //   std::cout << "    == ARG EFFECT TRACKING ==" << std::endl;
//   //   for (auto & ele : argTrackRes) {
//   //     auto currFun = ele.first;
//   //     auto currFunData = ele.second;
//   //     std::cout << "      Function: " << currFun->getName().str() << std::endl;
//   //     for (auto & data : currFunData) {
//   //       unsigned argIdx = data.first;
//   //       auto calledFuns = data.second; 
//   //       std::cout << "        [arg" << argIdx << "]: ";
//   //       for (auto & funName : calledFuns) {
//   //         std::cout << funName << " -- ";
//   //       }
//   //       std::cout << std::endl;
//   //     }
//   //   }
//   //   #endif

//   //   auto funcCallBFRes = MM.getFunctionCallBreathFirstRes();
//   //   for (auto & ele : funcCallBFRes) {
//   //     auto currFunName = ele.first().str();
//   //     auto currFunData = ele.second;
//   //     if (currFunName == mainFunName) {
//   //       std::vector<std::set<std::string>> fc;
//   //       for (auto & e : currFunData) fc.push_back(e.getFunctionSet());
//   //       funCallBFData[c] = fc;
//   //       break;
//   //     }
//   //   }

//   //   #if DEBUG_CHECKPOINTS == 1
//   //   std::cout << "    == FUN CALL BF ==" << std::endl;
//   //   for (auto & ele : funcCallBFRes) {
//   //     auto currFunName = ele.first().str();
//   //     auto currFunData = ele.second;
//   //     std::cout << "      " << currFunName;
//   //     if (currFunName == mainFunName) {
//   //       std::cout << " [MAIN]";
//   //     }
//   //     std::cout << std::endl;
//   //     unsigned i = 0;
//   //     for (auto & e : currFunData) {
//   //       std::cout << "        " << ++i << ": " << e.getNodeCompressedName() << std::endl;
//   //     }
//   //   }

//   //   #endif

//   //   UNPROTECT(1);


//   // });
//   // call(contextsVec, weightAnalysis, simpleArgumentAnalysis, funCallBFData);
// }

// void compareContexts(std::vector<rir::Context> & contextsVec, ComparisonCallback call) {
//   bool noStrictComparison = getenv("DISABLE_STRICT_COMPARISON") ? true : false;
//   bool noRoughComparison = getenv("DISABLE_ROUGH_COMPARISON") ? true : false;
//   for (auto it_currCon = contextsVec.begin(); it_currCon != contextsVec.end(); ++it_currCon) {
//     auto it_other = it_currCon + 1;
//     while (it_other != contextsVec.end()) {
//       auto currCon = *it_currCon;
//       auto other = *it_other;
      
//       // Strictly comparable
//       if (noStrictComparison == false && other.smaller(currCon)) {
//         call(currCon, other, ComparisonType::STRICT);
//       } else if (noStrictComparison == false && currCon.smaller(other)) {
//         call(other, currCon, ComparisonType::STRICT);
//       }
//       // Roughly comparable - EQ
//       else if (noRoughComparison == false && other.roughlySmaller(currCon) && currCon.roughlySmaller(other)) {
//         call(currCon, other, ComparisonType::ROUGH_EQ);
//       }

//       // Roughlt comparable - NEQ
//       else if (noRoughComparison == false && other.roughlySmaller(currCon)) {
//         call(currCon, other, ComparisonType::ROUGH_NEQ);
//       } else if (noRoughComparison == false && currCon.roughlySmaller(other)) {
//         call(other, currCon, ComparisonType::ROUGH_NEQ);
//       }      
//       it_other++;
//     }
//   }
// }

