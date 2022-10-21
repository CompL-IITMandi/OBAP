#include "utils/SerializedDataProcessor.h"

#include <ostream>
#include <iostream>
#include <algorithm>
#include "utils/serializerData.h"
#include "utils/TVGraph.h"
#include "utils/UMap.h"
#include "opt/ModuleManager.h"
#include "utils/OBAHolder.h"

#include "utils/deserializerData.h"

#define DEBUG_LOCALS_INIT 0

// Possible values 0, 1, 2
#define DEBUG_CONTEXTWISE_SIMILARITY_CHECK 0
#define DEBUG_CONTEXTWISE_TYPE_VERSIONING 0


#define PRINT_REPRESENTATIVE_SELECTION_NON_TRIVIAL_CASES 0


size_t SerializedDataProcessor::bitcodesSeen = 0;
size_t SerializedDataProcessor::bitcodesDeprecated = 0;
unsigned int SerializedDataProcessor::binariesWithScopeForReduction = 0;
unsigned int SerializedDataProcessor::stirctComparisons = 0;
unsigned int SerializedDataProcessor::roughEQComparisons = 0;
unsigned int SerializedDataProcessor::roughNEQComparisons = 0;
unsigned int SerializedDataProcessor::TVCases = 0;

unsigned int SerializedDataProcessor::overallTotalTV = 0;

unsigned int SerializedDataProcessor::getNumContexts() {
  return _reducedContextWiseData.size();
}


void SerializedDataProcessor::populateOffsetUnit(SEXP ouContainer) {
  // 
  // ouContainer is expecting getNumContexts() number of contexts to be present
  // 

  // An offset unit contains contextUnits
  int ouIdx = rir::offsetUnit::contextsStartingIndex();
  for (auto & ele : _reducedContextWiseData) {
    // Case: V = 0, Just one binary
    if (ele.second.size() == 1) {

      rir::Protect protecc;

      SEXP cuContainer;
      protecc(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(1)));
      rir::contextUnit::addContext(cuContainer, ele.first);
      rir::contextUnit::addVersioning(cuContainer, 0);
      rir::contextUnit::addTFSlots(cuContainer, R_NilValue);

      std::pair<SEXP, SEXP> data = ele.second[0];
      SEXP buContainer;
      protecc(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
      rir::binaryUnit::addEpoch(buContainer, data.first);
      rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
      rir::binaryUnit::addTVData(buContainer, R_NilValue);
      

      // Add Binary unit to Context unit
      rir::generalUtil::addSEXP(cuContainer, buContainer, rir::contextUnit::binsStartingIndex());


      // Add Context unit to Offset unit
      rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);

    } else if (_tvGraphData.find(ele.first) != _tvGraphData.end()) {

      // TVCases++;

      TVGraph tvg = _tvGraphData[ele.first];
      auto numTypeVersions = tvg.getNumTypeVersions();

      
      if (numTypeVersions == 1) {
        tvg.iterateOverTVs([&] (std::vector<uint32_t> slotData, std::vector<SEXP> generalSlotData, TVNode node) {

          auto nodeRes = node.get();

          // If number of type versions is one and the number of binaries is also one, V = 0
          if (nodeRes.size() == 1) {

            rir::Protect protecc;

            SEXP cuContainer;
            protecc(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(1)));
            rir::contextUnit::addContext(cuContainer, ele.first);
            rir::contextUnit::addVersioning(cuContainer, 0);
            rir::contextUnit::addTFSlots(cuContainer, R_NilValue);

            std::pair<SEXP, SEXP> data = nodeRes[0];
            SEXP buContainer;
            protecc(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
            rir::binaryUnit::addEpoch(buContainer, data.first);
            rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
            rir::binaryUnit::addTVData(buContainer, R_NilValue);

            // Add Binary unit to Context unit
            rir::generalUtil::addSEXP(cuContainer, buContainer, rir::contextUnit::binsStartingIndex());


            // Add Context unit to Offset unit
            rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);
          } else {

            rir::Protect protecc;

            // If number of type versions is one and the number of binaries is > 1, V = 1
            SEXP cuContainer;
            protecc(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(nodeRes.size())));
            rir::contextUnit::addContext(cuContainer, ele.first);
            rir::contextUnit::addVersioning(cuContainer, 1);
            rir::contextUnit::addTFSlots(cuContainer, R_NilValue);

            int startIdx = rir::contextUnit::binsStartingIndex();
            for (auto & data : nodeRes) {
              SEXP buContainer;
              rir::Protect protecc1;

              protecc1(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
              rir::binaryUnit::addEpoch(buContainer, data.first);
              rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
              rir::binaryUnit::addTVData(buContainer, R_NilValue);

              // Add Binary unit to Context unit
              rir::generalUtil::addSEXP(cuContainer, buContainer, startIdx);
              startIdx++;
            }

            // Add Context unit to Offset unit
            rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);

          }
          
        });
      } else {
        rir::Protect protecc;
        SEXP cuContainer;
        protecc(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(tvg.getBinariesCount())));
        rir::contextUnit::addContext(cuContainer, ele.first);
        
        rir::contextUnit::addVersioning(cuContainer, 2);

        auto tvSolution = tvg.getSolutionSorted();

        std::vector<int> genericFeedback;
        std::vector<int> typeFeedback;

        for (auto & slotIdx : tvSolution) {
          if (slotIdx >= tvg.genericFeedbackLen) {
            typeFeedback.push_back(slotIdx);
          } else {
            genericFeedback.push_back(slotIdx);
          }
        }

        rir::contextUnit::addTFSlots(cuContainer, typeFeedback);

        rir::contextUnit::addFBSlots(cuContainer, genericFeedback);

        int startIdx = rir::contextUnit::binsStartingIndex();
        tvg.iterateOverTVs([&] (std::vector<uint32_t> slotData, std::vector<SEXP> generalSlotData, TVNode node) {
          auto nodeRes = node.get();
          for (auto & data : nodeRes) {
            SEXP buContainer;
            rir::Protect protecc1;
            protecc1(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
            rir::binaryUnit::addEpoch(buContainer, data.first);
            rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
            rir::binaryUnit::addTVData(buContainer, slotData);
            rir::binaryUnit::addFBData(buContainer, generalSlotData);

            // Add Binary unit to Context unit
            rir::generalUtil::addSEXP(cuContainer, buContainer, startIdx);
            startIdx++;
          }
        });

        // Add Context unit to Offset unit
        rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);
      }
    } else {
      Rf_error("Invalid case while creating deserializer unit!");
    }

    ouIdx++;
  }

}

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

static std::pair<SEXP, SEXP> chooseRepresentativeRev(std::vector<std::pair<SEXP, SEXP>> possibilities) {
  std::pair<SEXP, SEXP> representative;

  int currMax = -1;

  for (unsigned int i = 0; i < possibilities.size(); i++) {
    SEXP cData = possibilities[i].second;
    SEXP rData = rir::contextData::getReqMapAsVector(cData);
    int size = Rf_length(rData);

    // First case, C++ pleae optimize this, unroll and magic!
    if (currMax == -1) {
      representative.first = possibilities[i].first;
      representative.second = possibilities[i].second;
      currMax = size;
      continue;
    }

    // Update current min
    if (size > currMax) {
      representative.first = possibilities[i].first;
      representative.second = possibilities[i].second;
      currMax = size;
    }
  }
  return representative;

}


static std::pair<SEXP, SEXP> chooseLastSeen(std::vector<std::pair<SEXP, SEXP>> possibilities) {
  std::pair<SEXP, SEXP> representative;

  int currMaxEpoch = 0;

  for (unsigned int i = 0; i < possibilities.size(); i++) {
    SEXP epoch = possibilities[i].first;
    unsigned long currEpoch = std::stoul(CHAR(PRINTNAME(epoch)));

    if (currEpoch > currMaxEpoch) {
      currMaxEpoch = currEpoch;
      representative.first = possibilities[i].first;
      representative.second = possibilities[i].second;
    }
  }
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

  // Count binaries that have scope for reduction
  for (auto & ele : _origContextWiseData) {
    if (ele.second.size() > 1) {
      binariesWithScopeForReduction += ele.second.size();
    }
  }


  // 
  // 2. Contextwise binary reduction
  // 

  bool skipBinaryReduction = getenv("SKIP_CONTEXTWISE_REDUCTION") ? getenv("SKIP_CONTEXTWISE_REDUCTION")[0] == '1' : false;

  int dumbOBAP = getenv("DUMBOBAP") ? std::stoi(getenv("DUMBOBAP")) : 0;

  // 
  // DUMB OBAP SETTINGS
  //  0 -> OFF
  //  1 -> Only last seen
  //  2 -> One with smallest requirement map
  //  3 -> One with largest requirement map
  // 

  if (dumbOBAP > 0) {

    for (auto & ele : _origContextWiseData) {
      unsigned long con = ele.first;
      switch(dumbOBAP) {
        case 1: {
          _reducedContextWiseData[con].push_back(chooseLastSeen(ele.second));
          break;
        }
        case 2: {
          _reducedContextWiseData[con].push_back(chooseRepresentative(ele.second));
          break;
        }
        case 3: {
          _reducedContextWiseData[con].push_back(chooseRepresentativeRev(ele.second));
          break;
        }
        default: {}
      }
    }

  } else {
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

      // std::cout << "Context check" << std::endl;

      std::vector<unsigned int> removed;
      std::vector<std::pair<SEXP, SEXP>> groups;
      for (unsigned int i = 0; i < cDataVec.size(); i++) {
        if (std::find(removed.begin(), removed.end(), i) != removed.end()) {
          continue;
        }

        std::vector<std::pair<SEXP, SEXP>> similars;

        // Atlease one element will be there as its similar to itself.
        similars.push_back(cDataVec[i]);
        
        std::stringstream pref1;
        pref1 << _pathPrefix << CHAR(PRINTNAME(cDataVec[i].first));
        OBAHolder r1(pref1.str(), cDataVec[i].second);

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
          OBAHolder r2(pref2.str(), cDataVec[j].second);

          // std::cout << "numArguments: " << (r1.getFS().numArguments == r2.getFS().numArguments) << std::endl;

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
          if (skipBinaryReduction == false && cRes.similar && argSimilar) {
            // We only deprecate if one is dispatchable in the absence of other, if they are exclusive we might run into theoretical worst case where
            // optimistic unlock never happens
            if (std::includes(r1.reqMap.begin(), r1.reqMap.end(), r2.reqMap.begin(), r2.reqMap.end()) || 
                std::includes(r2.reqMap.begin(), r2.reqMap.end(), r1.reqMap.begin(), r1.reqMap.end())) {
              removed.push_back(j);
              _deprecatedBitcodes++;

              similars.push_back(cDataVec[j]);

              #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 1
              std::cout << " [SIMILAR]";
              #endif
            }

          }

          #if DEBUG_CONTEXTWISE_SIMILARITY_CHECK > 1
          std::cout << std::endl;
          #endif
        }

        auto representative = chooseRepresentative(similars);

        _reducedContextWiseData[con].push_back(representative);
      }
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

    std::cout << "FOR CONTEXT(" << ele.first << "): " << std::endl;

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

      overallTotalTV += g.getNumTypeVersions();

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
      std::cout << "NUMBER OF TYPE VERSIONS: NA (Only one binary)" << std::endl;
      #if DEBUG_CONTEXTWISE_TYPE_VERSIONING > 0
      printSpace(8);
      std::cout << "(*) TV not needed" << std::endl;
      #endif
    }
    
    
  }
  // 
  // 4. Context Curbing
  // 

  bool skipContextMasking = getenv("SKIP_CONTEXT_MASKING") ? getenv("SKIP_CONTEXT_MASKING")[0] == '1' : false;

  std::vector<unsigned long> contextsWithOneBinary;
  std::set<unsigned long> redundantContexts;

  if (!skipContextMasking) {
    for (auto & ele : _reducedContextWiseData) {
      if (ele.second.size() == 1) {
        contextsWithOneBinary.push_back(ele.first);
      }
    }
  }


  for (int i = 0; i < contextsWithOneBinary.size(); i++) {

    auto currC1 = contextsWithOneBinary[i];
    
    // We know there is only one element
    std::pair<SEXP, SEXP> c1Ele = _reducedContextWiseData[currC1][0];

    std::stringstream pref1;
    pref1 << _pathPrefix << CHAR(PRINTNAME(c1Ele.first));
    OBAHolder r1(pref1.str(), c1Ele.second);

    for (int j = i + 1; j < contextsWithOneBinary.size(); j++) {

      auto currC2 = contextsWithOneBinary[j];

      // We know there is only one element
      std::pair<SEXP, SEXP> c2Ele = _reducedContextWiseData[currC2][0];

      std::stringstream pref2;
      pref2 << _pathPrefix << CHAR(PRINTNAME(c2Ele.first));
      OBAHolder r2(pref2.str(), c2Ele.second);

      auto cRes = r1.equals(r2);
      bool argSimilar = cRes.argEffectResult <= 2;
      
      if (cRes.similar && argSimilar) {
        // The two context binaries are similar, now curb them

        auto currCon = rir::Context(currC1);
        auto other = rir::Context(currC2);
        
        // 
        // Strictly comparable
        // 
        // We will be able to deprecate contexts only in this case
        // 
        if (other.smaller(currCon)) {
          // Here other is more specialized than currCon
          redundantContexts.insert(other.toI());
          _mask.addMaskC1MinusC2(other, currCon);

          stirctComparisons++;
          
        } else if (currCon.smaller(other)) { 
          // Here currCon is more specialized than other
          redundantContexts.insert(currCon.toI());
          _mask.addMaskC1MinusC2(currCon, other);
        
          stirctComparisons++;
        }
        // Roughly comparable - EQ
        else if (other.roughlySmaller(currCon) && currCon.roughlySmaller(other)) {
          // Trivial case, generate mask
          _mask.addMaskC1UQUC2(currCon, other);
          
          roughEQComparisons++;
        }
        // Roughlt comparable - NEQ
        else if (other.roughlySmaller(currCon)) {
          _mask.addMaskC1TYPC2(other, currCon);

          roughNEQComparisons++;
          
        } else if (currCon.roughlySmaller(other)) {
          _mask.addMaskC1TYPC2(currCon, other);

          roughNEQComparisons++;
        }

        // // We only deprecate if one is dispatchable in the absence of other, if they are exclusive we might run into theoretical worst case where
        // // optimistic unlock never happens
        // if (std::includes(r1.reqMap.begin(), r1.reqMap.end(), r2.reqMap.begin(), r2.reqMap.end()) || 
        //     std::includes(r2.reqMap.begin(), r2.reqMap.end(), r1.reqMap.begin(), r1.reqMap.end())) {
        //   removed.push_back(j);
        
        // }
      }

    }
    
  }


  // Collect stats
  bitcodesSeen += _origBitcodes;
  bitcodesDeprecated += _deprecatedBitcodes;
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
    std::cout << "Processed Contexts" << std::endl;
  }

  unsigned int finalBins = 0;

  for (auto & ele : _reducedContextWiseData) {
    printSpace(space + 2);
    if (_tvGraphData.find(ele.first) != _tvGraphData.end()) {
      finalBins += _tvGraphData[ele.first].getBinariesCount();

      std::cout << "├─(" << ele.first << "): " << _tvGraphData[ele.first].getBinariesCount() << " binaries" << std::endl;
      _tvGraphData[ele.first].print(space + 4);
    } else {
      finalBins += ele.second.size();
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


void SerializedDataProcessor::printStats(const unsigned int & space) {
  printSpace(space);
  std::cout << "Total seen                : " << bitcodesSeen << std::endl;
  printSpace(space);
  std::cout << "Total scope for reduction : " << binariesWithScopeForReduction << std::endl;

  printSpace(space);
  std::cout << "Binaries Reduced          : " << bitcodesDeprecated << std::endl;

  printSpace(space);
  std::cout << "overallTotalTV            : " << overallTotalTV << std::endl; 

  printSpace(space);
  std::cout << "Strict                    : " << stirctComparisons << std::endl;
  printSpace(space);
  std::cout << "Rough EQ                  : " << roughEQComparisons << std::endl;
  printSpace(space);
  std::cout << "Rough NEQ                 : " << roughNEQComparisons << std::endl;

  // printSpace(space);
  // std::cout << "TVCases                   : " << TVCases << std::endl;

}