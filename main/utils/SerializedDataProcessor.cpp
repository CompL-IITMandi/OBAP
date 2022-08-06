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

      SEXP cuContainer;
      PROTECT(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(1)));
      rir::contextUnit::addContext(cuContainer, ele.first);
      rir::contextUnit::addVersioning(cuContainer, 0);
      rir::contextUnit::addTFSlots(cuContainer, R_NilValue);

      std::pair<SEXP, SEXP> data = ele.second[0];
      SEXP buContainer;
      PROTECT(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
      rir::binaryUnit::addEpoch(buContainer, data.first);
      rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
      rir::binaryUnit::addTVData(buContainer, R_NilValue);
      

      // Add Binary unit to Context unit
      rir::generalUtil::addSEXP(cuContainer, buContainer, rir::contextUnit::binsStartingIndex());

      UNPROTECT(1);

      // Add Context unit to Offset unit
      rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);
      UNPROTECT(1);

    } 
    else if (_tvGraphData.find(ele.first) != _tvGraphData.end()) {

      TVGraph tvg = _tvGraphData[ele.first];
      auto numTypeVersions = tvg.getNumTypeVersions();

      
      if (numTypeVersions == 1) {
        tvg.iterateOverTVs([&] (std::vector<uint32_t> slotData, TVNode node) {

          auto nodeRes = node.get();

          // If number of type versions is one and the number of binaries is also one, V = 0
          if (nodeRes.size() == 1) {
            SEXP cuContainer;
            PROTECT(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(1)));
            rir::contextUnit::addContext(cuContainer, ele.first);
            rir::contextUnit::addVersioning(cuContainer, 0);
            rir::contextUnit::addTFSlots(cuContainer, R_NilValue);

            std::pair<SEXP, SEXP> data = nodeRes[0];
            SEXP buContainer;
            PROTECT(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
            rir::binaryUnit::addEpoch(buContainer, data.first);
            rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
            rir::binaryUnit::addTVData(buContainer, R_NilValue);

            // Add Binary unit to Context unit
            rir::generalUtil::addSEXP(cuContainer, buContainer, rir::contextUnit::binsStartingIndex());

            UNPROTECT(1);

            // Add Context unit to Offset unit
            rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);
            UNPROTECT(1);
          } else {

            // If number of type versions is one and the number of binaries is > 1, V = 1
            SEXP cuContainer;
            PROTECT(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(nodeRes.size())));
            rir::contextUnit::addContext(cuContainer, ele.first);
            rir::contextUnit::addVersioning(cuContainer, 1);
            rir::contextUnit::addTFSlots(cuContainer, R_NilValue);

            int startIdx = rir::contextUnit::binsStartingIndex();
            for (auto & data : nodeRes) {
              SEXP buContainer;
              PROTECT(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
              rir::binaryUnit::addEpoch(buContainer, data.first);
              rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
              rir::binaryUnit::addTVData(buContainer, R_NilValue);

              // Add Binary unit to Context unit
              rir::generalUtil::addSEXP(cuContainer, buContainer, startIdx);
              startIdx++;
              UNPROTECT(1);
            }

            // Add Context unit to Offset unit
            rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);
            UNPROTECT(1);

          }
          
        });
      } else {
        SEXP cuContainer;
        PROTECT(cuContainer = Rf_allocVector(VECSXP, rir::contextUnit::getContainerSize(tvg.getBinariesCount())));
        rir::contextUnit::addContext(cuContainer, ele.first);
        
        rir::contextUnit::addVersioning(cuContainer, 2);
        
        rir::contextUnit::addTFSlots(cuContainer, tvg.getSolutionSorted());


        int startIdx = rir::contextUnit::binsStartingIndex();
        tvg.iterateOverTVs([&] (std::vector<uint32_t> slotData, TVNode node) {
          auto nodeRes = node.get();
          for (auto & data : nodeRes) {
            SEXP buContainer;
            PROTECT(buContainer = Rf_allocVector(VECSXP, rir::binaryUnit::getContainerSize()));
            rir::binaryUnit::addEpoch(buContainer, data.first);
            rir::binaryUnit::addReqMap(buContainer, rir::contextData::getReqMapAsVector(data.second));
            rir::binaryUnit::addTVData(buContainer, slotData);

            // Add Binary unit to Context unit
            rir::generalUtil::addSEXP(cuContainer, buContainer, startIdx);
            startIdx++;
            UNPROTECT(1);
          }
        });

        // Add Context unit to Offset unit
        rir::generalUtil::addSEXP(ouContainer, cuContainer, ouIdx);
        UNPROTECT(1);
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

    // std::cout << "Context check" << std::endl;

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
