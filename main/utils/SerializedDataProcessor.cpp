#include "utils/SerializedDataProcessor.h"

#include <ostream>
#include <iostream>
#include <algorithm>
#include "utils/serializerData.h"
#include "utils/TVGraph.h"
#include "utils/UMap.h"
#include "opt/ModuleManager.h"
#include "utils/OBAHolder.h"

void SerializedDataProcessor::init() {
  static SEXP maskSym = Rf_install("mask");

  std::vector<std::pair<SEXP, SEXP>> cDataVec;

  REnvHandler contextMapHandler(_cData);
  contextMapHandler.iterate([&] (SEXP epochSym, SEXP cData) {
    if (epochSym == maskSym) return;

    // rir::contextData::print(cData, 4);

    _origBitcodes++;
    unsigned long con = rir::contextData::getContext(cData);
    _origContextWiseData[con].push_back(cData);

    cDataVec.push_back(std::pair<SEXP, SEXP>(epochSym, cData));
  });


  // std::vector<unsigned int> removed;

  for (unsigned int i = 0; i < cDataVec.size(); i++) {

    // Prefix  : HAST_OFFSET_EPOCH
    // Bitcode : HAST_OFFSET_EPOCH.bc
    // Pool    : HAST_OFFSET_EPOCH.bc
    std::stringstream pref;
    pref << _pathPrefix << CHAR(PRINTNAME(cDataVec[i].first));
    OBAHolder r1(pref.str());

    r1.print(4);


    // for (unsigned int j = i; j < cDataVec.size(); j++) {
    //   // removed.push_back(j);
    // }
  }

  // for (auto & ele : _origContextWiseData) {
  //   printSpace(4);
  //   std::cout << "Processing context(" << ele.first << "): " << rir::Context(ele.first) << std::endl;

  //   // TVGraph g(ele.second);
  //   // auto stat = g.init();
  //   // g.print(6);

  //   // if (!stat) {
  //   //   Rf_error("Init failed for TVGraph");
  //   // }
  // }

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

  printSpace(space);
  std::cout << "Original Contexts count : " << _origContextWiseData.size() << std::endl;

  printSpace(space);
  std::cout << "Final Contexts count    : " << (_origContextWiseData.size() - _deprecatedContexts) << std::endl;

  printSpace(space);
  std::cout << "Original Bitcodes count : " << _origBitcodes << std::endl;

  printSpace(space);
  std::cout << "Final Bitcodes count    : " << (_origBitcodes - _deprecatedBitcodes) << std::endl;
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

