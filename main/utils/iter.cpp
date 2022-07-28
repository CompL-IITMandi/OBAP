#include "utils/iter.h"

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"
#include "opt/ModuleManager.h"
#include <algorithm>
#include "utils/serializerData.h"
#include <thread>

#include <chrono>
using namespace std::chrono;


#define DEBUG_ANALYSIS 1
#define DEBUG_SLOT_SELECTION 0
#define DEBUG_SLOT_SELECTION_READABLE 0

#define PRING_CSV_DATA 0

#define MAX_SLOTS 7

#define TICK_TICK_LAP_COUNT 1000000
#define MAX_POOL_SIZE 10000

typedef std::vector<rir::ObservedValues> TFVector;

inline TFVector getFeedbackAsVector(SEXP cData) {
  TFVector res;

  SEXP tfContainer = rir::contextData::getTF(cData);
  rir::ObservedValues * tmp = (rir::ObservedValues *) DATAPTR(tfContainer);
  for (int i = 0; i < Rf_length(tfContainer) / (int) sizeof(rir::ObservedValues); i++) {
      res.push_back(tmp[i]);      
  }

  return res;
}


// // https://stackoverflow.com/questions/5095407/all-combinations-of-k-elements-out-of-n
// template <typename Iterator>
// inline bool next_combination(const Iterator first, Iterator k, const Iterator last) {
//    /* Credits: Thomas Draper */
//    if ((first == last) || (first == k) || (last == k))
//       return false;
//    Iterator itr1 = first;
//    Iterator itr2 = last;
//    ++itr1;
//    if (last == itr1)
//       return false;
//    itr1 = last;
//    --itr1;
//    itr1 = k;
//    --itr2;
//    while (first != itr1)
//    {
//       if (*--itr1 < *itr2)
//       {
//          Iterator j = k;
//          while (!(*itr1 < *j)) ++j;
//          std::iter_swap(itr1,j);
//          ++itr1;
//          ++j;
//          itr2 = k;
//          std::rotate(itr1,j,last);
//          while (last != j)
//          {
//             ++j;
//             ++itr2;
//          }
//          std::rotate(k,itr2,last);
//          return true;
//       }
//    }
//    std::rotate(first,k,last);
//    return false;
// }

class CombinationsIndexArray {
    std::vector<int> index_array;
    int last_index;
    public:
    CombinationsIndexArray(int number_of_things_to_choose_from, int number_of_things_to_choose_in_one_combination) {
        last_index = number_of_things_to_choose_from - 1;
        for (int i = 0; i < number_of_things_to_choose_in_one_combination; i++) {
            index_array.push_back(i);
        }
    }
    int operator[](int i) {
        return index_array[i];
    }
    int size() {
        return index_array.size();
    }
    bool advance() {

        int i = index_array.size() - 1;
        if (index_array[i] < last_index) {
            index_array[i]++;
            return true;
        } else {
            while (i > 0 && index_array[i-1] == index_array[i]-1) {
                i--;
            }
            if (i == 0) {
                return false;
            } else {
                index_array[i-1]++;
                while (i < index_array.size()) {
                    index_array[i] = index_array[i-1]+1;
                    i++;
                }
                return true;
            }
        }
    }
};

// https://stackoverflow.com/questions/9330915/number-of-combinations-n-choose-r-in-c
unsigned nChoosek( unsigned n, unsigned k ){
    if (k > n) return 0;
    if (k * 2 > n) k = n-k;
    if (k == 0) return 1;

    int result = n;
    for( int i = 2; i <= k; ++i ) {
        result *= (n-i+1);
        result /= i;
    }
    return result;
}

class Ticker {
  public:
    Ticker(unsigned int totalCycles) : remainingCycles(totalCycles), total(totalCycles) {
      start = std::chrono::high_resolution_clock::now();
    }

    void lap(unsigned int completedCycles) {
      auto end = std::chrono::high_resolution_clock::now();
      auto duration = duration_cast<milliseconds>(end - start);
      remainingCycles = remainingCycles - completedCycles;
      double timeForOneCycleInMs = (double) duration.count() / completedCycles;
      averageTimePerThread += timeForOneCycleInMs;
      checks++;
      double remainingTotalTimeInMs = timeForOneCycleInMs * remainingCycles;
      auto completedPercent = (total - remainingCycles) / (double) total;
      std::cout << "=== TICK TICK ===" << std::endl;
      std::cout << "Done(" << (total - remainingCycles) << "/" << total << ")" << std::endl; 
      std::cout << "Completed: " << (completedPercent*100) << "%" << std::endl;       
      std::cout << "Remaining time estimate: " << (remainingTotalTimeInMs/(1000 * 60)) << " minutes" << std::endl;
      std::cout << "=================" << std::endl;

      start = std::chrono::high_resolution_clock::now();
    }

    unsigned int getAverageThreadTime() {
      if (checks == 0) return 0;
      return (unsigned int) (averageTimePerThread / checks);
    }

  private:
    double averageTimePerThread = 0;
    int checks = 0;
    std::chrono::high_resolution_clock::time_point start;
    unsigned int total, remainingCycles, tocks;

};


class TVGraph {

  class TVNode {
    public:
      TVNode() = delete;
      TVNode(const unsigned int & idx) : feedbackVectorIdx(idx) {}
      
      void addNode(SEXP node) {
        nodes.push_back(node);
      }

      int size() {
        return nodes.size();
      }

      unsigned int getFeedbackVectorIdx() {
        return feedbackVectorIdx;
      }

    private:
      unsigned int feedbackVectorIdx;
      std::vector<SEXP> nodes;
  };
  
  public:

    TVGraph(std::vector<SEXP> v) {
      for (auto & ele : v) {
        addNode(ele);
      }
    }

    void addNode(SEXP cData) {
      TFVector curr = getFeedbackAsVector(cData);
      // If curr exists then add to an existing node
      int i = 0;
      for (; i < typeVersions.size(); i++) {
        if (TFVEquals(typeVersions[i], curr)) {
          nodes[i].addNode(cData);
          return;
        }
      }

      // If this is a new version, add new
      typeVersions.push_back(curr);
      TVNode n(typeVersions.size() - 1);
      n.addNode(cData);
      nodes.push_back(n);
    }

    // Returns true if the given indexes uniquely differentiate between type versions
    bool checkValidity(std::vector<int> & indices, bool printDebug=false) {
      std::vector<std::vector<uint32_t>> res;

      #if DEBUG_SLOT_SELECTION == 1
      std::cout << "=== COMPARISONS ===" << std::endl;
      #endif

      for (auto & tv : typeVersions) {
        std::vector<uint32_t> ele;

        #if DEBUG_SLOT_SELECTION == 1
        std::cout << "( ";
        #endif

        for (auto & idx : indices) {
          uint32_t curr = *((uint32_t *) &(tv[idx]));
          ele.push_back(curr);

          #if DEBUG_SLOT_SELECTION == 1
          tv[idx].print(std::cout);
          std::cout << " ; ";
          #endif
        }

        #if DEBUG_SLOT_SELECTION == 1
        std::cout << ")" << std::endl;
        #endif

        res.push_back(ele);
      }

      #if DEBUG_SLOT_SELECTION == 1
      std::cout << "=== === === === ===" << std::endl;
      #endif

      // Check if duplicate elements exist
      for (int m = 0; m < res.size(); m++) {
        auto first = res[m];

        for (int i = 0; i < res.size(); i++) {
          if (i == m) continue;
          auto curr = res[i];

          bool match = true;

          for (int j = 0; j < first.size(); j++) {
            if (first[j] != curr[j]) {
              match = false;
            }
          }

          
          // If match is true, then return false as we cannot uniquely identify using this indices subset
          if (match) {
            #if DEBUG_SLOT_SELECTION == 1
            std::cout << "Result is same for " << m << " and " << i << std::endl; 
            #endif
            return false;
          }
        }
      }

      if (printDebug) {
        std::cout << "          === COMPARISONS ===" << std::endl;
        for (auto & tv : typeVersions) {
          std::cout << "          ( ";
          for (auto & idx : indices) {
            tv[idx].print(std::cout);
            std::cout << " ; ";
          }
          std::cout << ")" << std::endl;
        }
        std::cout << "          Result: [ ";
        for (auto & idx : indices) {
          std::cout << idx << " ";
        }
        std::cout << "]" << std::endl;
        std::cout << "          === === === === ===" << std::endl;
      }
      
      return true;
    }

    std::pair<bool, std::vector<int>> findSlotIn(size_t k) {
      std::vector<unsigned int> ints = getDiffSlots();
      std::sort(ints.begin(), ints.end(), 
          [&](const unsigned int & idx1, const unsigned int & idx2) {
            std::set<unsigned int> s1, s2;
            for (auto & currVer: typeVersions) {
              uint32_t c1 = *((uint32_t *) &currVer[idx1]);
              uint32_t c2 = *((uint32_t *) &currVer[idx2]);
              s1.insert(c1);
              s2.insert(c2);
            }
            return s1.size() < s2.size();
          });

      std::cout << "      [SORTED] Slots in focus(" << ints.size() << "): [ ";
      for (auto & ele : ints) {
        std::cout << ele << " ";
      }
      std::cout << "]" << std::endl;


      if (k == 1) {
        for (auto & ele : ints) {
          // std::cout << "Trying (" << ele << ")" << std::endl;
          std::vector<int> indices;
          indices.push_back(ele);
          bool res = checkValidity(indices);
          if (res) {
            return std::pair<bool, std::vector<int>>(true, res);
          }  
        }

        return std::pair<bool, std::vector<int>>(false, std::vector<int>());
      }

      if (ints.size() < k) {
        std::cout << "Early Quit for " << k << std::endl;
        return std::pair<bool, std::vector<int>>(false, std::vector<int>());
      }

      auto totalPossibilities = nChoosek(ints.size(), k);
      std::cout << "  TOTAL POSSIBILITIES = " << totalPossibilities << std::endl;
      bool TPM = true;
      int count = 0;

      Ticker tickTick(totalPossibilities);

      // std::vector<std::thread> threadPool;

      bool solnFound = false;
      std::vector<int> solnRes;

      std::vector<std::thread> threadPool;
      threadPool.reserve(MAX_POOL_SIZE);
      CombinationsIndexArray combos(ints.size(), k);
      do {
        
        std::vector<int> indices;
        for (int i = 0; i < combos.size(); i++) {
          indices.push_back(ints[combos[i]]);
        }

        if (threadPool.size() > MAX_POOL_SIZE) {
          for (auto & t : threadPool) {
            t.join();
          }
          threadPool.clear();
        }

        threadPool.emplace_back([&](std::vector<int> idi) {
          count++;
          bool res = checkValidity(idi);
          if (res) {
            solnFound = true;
            solnRes = idi;
          }
        }, indices);

        if (count > TICK_TICK_LAP_COUNT) {
          tickTick.lap(count);
          count=0;
        }

        if (solnFound) {
          break;
        }
      } while (combos.advance());

      for (auto & t: threadPool) {
        t.join();
      }

      if (solnFound) {
        return std::pair<bool, std::vector<int>>(true, solnRes);
      }
      return std::pair<bool, std::vector<int>>(false, std::vector<int>());
    }

    void print() {
      unsigned int i = 0;
      std::cout << "    Found " << typeVersions.size() << " Type Versions" << std::endl;
      for (auto & ele : typeVersions) {
        std::cout << "      Type version " << i << " (" << nodes[i].size() << " redundant nodes) " << ": {";
        i++;

        int k = 0;
        for (auto & e : ele) {
          std::cout << " (" << k++ << ")[";
          e.print(std::cout);
          std::cout << "]";
        }
        std::cout << "}" << std::endl;
      }
      #if PRING_CSV_DATA == 1

      auto numVersions = typeVersions.size();
      auto numSlots = typeVersions[0].size();
      for (int slot = 0; slot < numSlots; slot++) {
        std::cout << "SLOT_" << slot << ",";
      }
      std::cout << std::endl;
      for (int ver = 0; ver < numVersions; ver++) {
        auto currVer = typeVersions[ver];
        for (int slot = 0; slot < numSlots; slot++) {
          uint32_t curr = *((uint32_t *) &currVer[slot]);
          std::cout << curr << ",";
        }
        std::cout << std::endl;
      }

      #endif
      if (typeVersions.size() > 1) {
        auto diffSlots = getDiffSlots();
        std::cout << "      Slots in focus(" << diffSlots.size() << "): [ ";
        for (auto & ele : diffSlots) {
          std::cout << ele << " ";
        }
        std::cout << "]" << std::endl;
        for (int i = 1; i <= MAX_SLOTS; i++) {
          std::cout << "    Looking for " << i << " slot solution" << std::endl;
          auto result = findSlotIn(i);
          if (result.first) {
            std::cout << "        Found result in " << i << " slots" << std::endl;
            checkValidity(result.second, true);
            return;
          }
          std::cout << "        No result in " << i << " slots" << std::endl;
        }
        std::cout << "        No Result found (Tried until " << MAX_SLOTS << ")" << std::endl;
      }
    }

  private:

    std::vector<unsigned int> getDiffSlots() {
      std::set<unsigned int> res;
      auto numVersions = typeVersions.size();
      auto numSlots = typeVersions[0].size();
      for (int i = 0; i < numSlots; i++) {
        uint32_t diffSlot1Val = *((uint32_t *) &typeVersions[0][i]);
        for (int j = 1; j < numVersions; j++) {
          uint32_t curr = *((uint32_t *) &typeVersions[j][i]);
          if (curr != diffSlot1Val) {
            res.insert(i);
          }
        }
      }

      return std::vector<unsigned int>(res.begin(), res.end());
    }
    

    bool TFVEquals(TFVector & v1, TFVector & v2) {
      assert(v1.size() == v2.size());

      for (int i = 0; i < v1.size(); i++) {
        rir::ObservedValues ov1, ov2;
        ov1 = v1[i];
        ov2 = v2[i];
        uint32_t * t1, * t2;
        t1 = (uint32_t*) &ov1;
        t2 = (uint32_t*) &ov2;
        if (*t1 != *t2) {
          // std::cout << "Different: [";
          // ov1.print(std::cout);
          // std::cout << "] [";
          // ov2.print(std::cout);
          // std::cout << "]" << std::endl;
          return false;
        }
      }
      return true;
    }

    std::vector<TFVector> typeVersions;
    std::vector<TVNode> nodes;
};


void doAnalysisOverContexts(const std::string & pathPrefix, SEXP contextMap, AnalysisCallback call) {
  static SEXP maskSym = Rf_install("mask");
  std::unordered_map<unsigned long, std::vector<SEXP>> contexts;

  std::cout << "Processing: " << pathPrefix << std::endl;

  REnvHandler contextMapHandler(contextMap);
  contextMapHandler.iterate([&] (SEXP epochSym, SEXP cData) {
    if (epochSym == maskSym) return;
    // #if DEBUG_ANALYSIS == 1
    // std::cout << "EPOCH: " << CHAR(PRINTNAME(epochSym)) << std::endl;
    // rir::contextData::print(cData, 2);
    // #endif

    unsigned long con = rir::contextData::getContext(cData);

    contexts[con].push_back(cData);
  });


  for (auto & ele : contexts) {
    std::cout << "  Processing context: " << rir::Context(ele.first) << std::endl;
    TVGraph g(ele.second);
    
    g.print();

    // getchar();
  }

  // std::vector<rir::Context> contextsVec;
  // std::unordered_map<rir::Context, unsigned> weightAnalysis;
  // std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > simpleArgumentAnalysis;
  // std::unordered_map<rir::Context, std::vector<std::set<std::string>>> funCallBFData;

  // #if DEBUG_ANALYSIS == 1
  // std::cout << "Starting analysis over: " << pathPrefix << std::endl;
  // #endif


  // REnvHandler contextMapHandler(contextMap);
  // contextMapHandler.iterate([&] (SEXP contextSym, SEXP cData) {
  //   if (contextSym == maskSym) return;

  //   unsigned long con = rir::contextData::getContext(cData);
  //   rir::Context c(con);

    

  //   contextsVec.push_back(c);          
      
  //   std::stringstream bitcodePath, poolPath;
  //   bitcodePath << pathPrefix << CHAR(PRINTNAME(contextSym)) << ".bc";
  //   poolPath << pathPrefix << CHAR(PRINTNAME(contextSym)) << ".pool";

  //   #if DEBUG_ANALYSIS == 1
  //   std::cout << "    Analyzing: " << bitcodePath.str() << std::endl;
  //   #endif

  //   FILE *reader;
  //   reader = fopen(poolPath.str().c_str(),"r");

  //   if (!reader) {
  //     for (int i = 0; i < 10; i++) {
  //       sleep(1);
  //       // std::cout << "waiting to open: " << metadataPath.str() << std::endl;
  //       reader = fopen(poolPath.str().c_str(),"r");
  //       if (reader) break;
  //     }

  //     if (!reader) {
  //       std::cout << "unable to open " << poolPath.str() << std::endl;
  //       Rf_error("unable to open file!");
  //       return;
  //     }
  //   }

  //   // Initialize the deserializing stream
  //   R_inpstream_st inputStream;
  //   R_InitFileInPStream(&inputStream, reader, R_pstream_binary_format, NULL, R_NilValue);

  //   SEXP poolDataContainer;
  //   PROTECT(poolDataContainer = R_Unserialize(&inputStream));

  //   // load bitcode
  //   llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> mb = llvm::MemoryBuffer::getFile(bitcodePath.str().c_str());
  //   llvm::orc::ThreadSafeContext TSC = std::make_unique<llvm::LLVMContext>();

  //   // Load the memory buffer into a LLVM module
  //   llvm::Expected<std::unique_ptr<llvm::Module>> llModuleHolder = llvm::parseBitcodeFile(mb->get()->getMemBufferRef(), *TSC.getContext());

  //   // ModuleManager to run our passes on the bitcodes
  //   ModuleManager MM(*llModuleHolder.get().get());

  //   MM.runPasses();

  //   auto callSiteCounterRes = MM.getRshCallSiteCounterRes();

  //   unsigned weight = 0;
  //   for (auto & ele : callSiteCounterRes) {
  //     weight += ele.second * RshBuiltinWeights::getWeight(ele.first().str());
  //   }
    
  //   #if DEBUG_ANALYSIS == 1
  //   std::cout << "    == CALL SITE OPCODE SIMILARITY ==" << std::endl;
  //   for (auto & ele : callSiteCounterRes) {
  //     std::cout << "      " << ele.first().str() << " : " << ele.second << "[" << (ele.second * RshBuiltinWeights::getWeight(ele.first().str())) << "]" << std::endl;
  //   }
  //   std::cout << "        " << "Total : " << weight << std::endl;
  //   #endif

  //   weightAnalysis[c] = weight;

  //   std::string mainFunName(CHAR(STRING_ELT(VECTOR_ELT(rir::SerializedPool::getFNames(poolDataContainer), 0), 0)));

  //   // std::cout << c.toI() << " --> " << mainFunName << std::endl;


  //   auto argTrackRes = MM.getRshArgumentEffectSimpleRes();
  //   for (auto & ele : argTrackRes) {
  //     auto currFun = ele.first;
  //     auto currFunData = ele.second;
  //     if (currFun->getName().str() == mainFunName) {
  //       simpleArgumentAnalysis[c] = currFunData;
  //       break;
  //     }
  //   }

  //   #if DEBUG_ANALYSIS == 1
  //   std::cout << "    == ARG EFFECT TRACKING ==" << std::endl;
  //   for (auto & ele : argTrackRes) {
  //     auto currFun = ele.first;
  //     auto currFunData = ele.second;
  //     std::cout << "      Function: " << currFun->getName().str() << std::endl;
  //     for (auto & data : currFunData) {
  //       unsigned argIdx = data.first;
  //       auto calledFuns = data.second; 
  //       std::cout << "        [arg" << argIdx << "]: ";
  //       for (auto & funName : calledFuns) {
  //         std::cout << funName << " -- ";
  //       }
  //       std::cout << std::endl;
  //     }
  //   }
  //   #endif

  //   auto funcCallBFRes = MM.getFunctionCallBreathFirstRes();
  //   for (auto & ele : funcCallBFRes) {
  //     auto currFunName = ele.first().str();
  //     auto currFunData = ele.second;
  //     if (currFunName == mainFunName) {
  //       std::vector<std::set<std::string>> fc;
  //       for (auto & e : currFunData) fc.push_back(e.getFunctionSet());
  //       funCallBFData[c] = fc;
  //       break;
  //     }
  //   }

  //   #if DEBUG_ANALYSIS == 1
  //   std::cout << "    == FUN CALL BF ==" << std::endl;
  //   for (auto & ele : funcCallBFRes) {
  //     auto currFunName = ele.first().str();
  //     auto currFunData = ele.second;
  //     std::cout << "      " << currFunName;
  //     if (currFunName == mainFunName) {
  //       std::cout << " [MAIN]";
  //     }
  //     std::cout << std::endl;
  //     unsigned i = 0;
  //     for (auto & e : currFunData) {
  //       std::cout << "        " << ++i << ": " << e.getNodeCompressedName() << std::endl;
  //     }
  //   }

  //   #endif

  //   UNPROTECT(1);


  // });
  // call(contextsVec, weightAnalysis, simpleArgumentAnalysis, funCallBFData);
}



void compareContexts(std::vector<rir::Context> & contextsVec, ComparisonCallback call) {
  bool noStrictComparison = getenv("DISABLE_STRICT_COMPARISON") ? true : false;
  bool noRoughComparison = getenv("DISABLE_ROUGH_COMPARISON") ? true : false;
  for (auto it_currCon = contextsVec.begin(); it_currCon != contextsVec.end(); ++it_currCon) {
    auto it_other = it_currCon + 1;
    while (it_other != contextsVec.end()) {
      auto currCon = *it_currCon;
      auto other = *it_other;
      
      // Strictly comparable
      if (noStrictComparison == false && other.smaller(currCon)) {
        call(currCon, other, ComparisonType::STRICT);
      } else if (noStrictComparison == false && currCon.smaller(other)) {
        call(other, currCon, ComparisonType::STRICT);
      }
      // Roughly comparable - EQ
      else if (noRoughComparison == false && other.roughlySmaller(currCon) && currCon.roughlySmaller(other)) {
        call(currCon, other, ComparisonType::ROUGH_EQ);
      }

      // Roughlt comparable - NEQ
      else if (noRoughComparison == false && other.roughlySmaller(currCon)) {
        call(currCon, other, ComparisonType::ROUGH_NEQ);
      } else if (noRoughComparison == false && currCon.roughlySmaller(other)) {
        call(other, currCon, ComparisonType::ROUGH_NEQ);
      }      
      it_other++;
    }
  }
}


std::string GlobalData::bitcodesFolder = "";