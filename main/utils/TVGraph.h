#pragma once

#include "utils/serializerData.h"
#include "utils/Debug.h"
#include <algorithm>
#include <thread>
#include <vector>
#include <unordered_map>
#include <chrono>
using namespace std::chrono;

// Possible values are 0, 1, 2
#define DEBUG_BACKTRACKING_SOLVER 0
#define DEBUG_IMP_CHECKPOINTS 0

#define DEBUG_WEIRD_CASES 0

#define DEBUG_GENERAL_FEEDBACK_ADAPTER_IN 0
#define DEBUG_GENERAL_FEEDBACK_ADAPTER_OUT 0

// 
// Helper Classes
// ====================================================================================
// 

// Source: https://stackoverflow.com/questions/5076695/how-can-i-iterate-through-every-possible-combination-of-n-playing-cards
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
        while (i < (int)index_array.size()) {
          index_array[i] = index_array[i-1]+1;
          i++;
        }
        return true;
      }
    }
  }
};

class Ticker {
  public:
    Ticker(unsigned int totalCycles) : total(totalCycles), remainingCycles(totalCycles) {
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
    unsigned int total, remainingCycles;

};

class TVNode {
  public:
    int typeFeedbackLen = -1;
    int genericFeedbackLen = -1;
    void addNode(std::pair<SEXP, SEXP> cData) {
      redundantNodes++;

      // rir::contextData::print(cData.second, 0);

      SEXP otherFeedbackContainer = rir::contextData::getFBD(cData.second);
      genericFeedbackLen = Rf_length(otherFeedbackContainer);

      SEXP typeFeedbackContainer = rir::contextData::getTF(cData.second);
      typeFeedbackLen = Rf_length(typeFeedbackContainer) / (int) sizeof(rir::ObservedValues);

      // std::cout << "TF LEN: " << typeFeedbackLen << std::endl;
      // std::cout << "GF LEN: " << genericFeedbackLen<< std::endl;

      auto rData = rir::contextData::getReqMapAsVector(cData.second);
      std::vector<std::string> reqMapVec = getReqMapAsCppVector(rData);
      std::stringstream ss;

      for (auto & e : reqMapVec) {
        ss << e << ";";
      }

      diversions[ss.str()].push_back(cData);
    }

    int getGeneralFeedbackLen() {
      assert(genericFeedbackLen != -1);
      return genericFeedbackLen;
    }

    int getTypeFeedbackLen() {
      assert(typeFeedbackLen != -1);
      return typeFeedbackLen;
    }
    int size() {
      return redundantNodes;
    }
    
    std::vector<std::pair<SEXP, SEXP>> get() {
      std::vector<std::pair<SEXP, SEXP>> result;

      for (auto & ele : diversions) {
        // For each diversion case, if the number of elements is > 1, select one
        result.push_back(ele.second[0]);
      }

      // Sort the result in the order of set subset property
      std::sort(result.begin(), result.end(), [&] (std::pair<SEXP, SEXP> first, std::pair<SEXP, SEXP> second) {
        std::set<std::string> s1(getReqMapAsCppSet(rir::contextData::getReqMapAsVector(first.second)));
        std::set<std::string> s2(getReqMapAsCppSet(rir::contextData::getReqMapAsVector(second.second)));

        // S1 is a subset of S2
        return std::includes(s2.begin(), s2.end(), s1.begin(), s1.end());
      });

      return result;
    }

    void print(unsigned int space = 0) {
      // printSpace(space);
      // std::cout << "=== TVNODE ===" << std::endl;
      
      // printSpace(space);
      // std::cout << "Diversions: " << diversions.size() << " unique req maps" << std::endl;

      #if DEBUG_WEIRD_CASES == 1
      printSpace(space);
      std::cout << "Debug Weird Cases: " << std::endl;
      for (auto & div : diversions) {
        printSpace(space + 2);
        if (div.second.size() > 1) {
          std::cout << "(WEIRD CASE): " << div.first << std::endl;
          for (auto & e : div.second) {
            rir::contextData::print(e.second, space + 4);
          }
        } else {
          std::cout << "(NORMAL CASE): " << div.first << std::endl;
        } 
      }
      #endif
      
      // printSpace(space);
      // std::cout << "Sorted binaries: " << std::endl;
      // auto result = get();
      // printSpace(space + 2);
      // for (int i = 0; i < result.size(); i++) {
      //   auto ele = result[i];
      //   std::vector<std::string> re(getReqMapAsCppVector(rir::contextData::getReqMapAsVector(ele.second)));
      //   std::cout << "[ ";
      //   for (auto & r : re) {
      //     std::cout << r << " ";
      //   }
      //   std::cout << "]";
      //   if (i + 1 != result.size()) {
      //     std::cout << " () ";
      //   }
      // }
      // std::cout << std::endl;

      // printSpace(space);
      // std::cout << "==============" << std::endl;

      // auto result = get();
      // printSpace(space);
      // std::cout << "└─(" << result.size() << " sorted binaries): " << diversions.size() << " diversions" << std::endl;
    }
  private:
    std::unordered_map<std::string, std::vector<std::pair<SEXP, SEXP>>> diversions;
    unsigned int redundantNodes = 0;
    
    std::vector<std::string> getReqMapAsCppVector(SEXP rData) {
      std::vector<std::string> reqMapVec;

      for (int i = 0; i < Rf_length(rData); i++) {
        SEXP ele = VECTOR_ELT(rData, i);
        reqMapVec.push_back(CHAR(PRINTNAME(ele)));
      }

      std::sort(reqMapVec.begin(), reqMapVec.end());

      return reqMapVec;
    }

    std::set<std::string> getReqMapAsCppSet(SEXP rData) {
      std::set<std::string> reqSet;

      for (int i = 0; i < Rf_length(rData); i++) {
        SEXP ele = VECTOR_ELT(rData, i);
        reqSet.insert(CHAR(PRINTNAME(ele)));
      }

      return reqSet;
    }

    
};

class TVGraph {
    static unsigned int MAX_SLOTS_SIZE;

  public:
    typedef std::vector<rir::ObservedValues> TFVector;
    typedef std::pair<int, int> WorklistElement;
    typedef std::vector<std::pair<int, int>> Worklist;
    typedef std::set<int> SolutionBucket;

    // Indirections to support additional type of feedbacks
    // For convert [HAST, INDEX] -> HAST_INDEX symbol and assign a unique number to the same
    static std::unordered_map<SEXP, unsigned int> feedbackIndirections;
    static int indIdx;

    std::set<int> blacklist;

    unsigned int SLOT_FINDER_BUDGET = 10;

    TVGraph() {}

    // 
    // Expects a vector of contextData SEXP
    // 
    TVGraph(std::vector<std::pair<SEXP, SEXP>> & cDataVec, std::set<int> bl) {
      blacklist = bl;
      for (auto & cData : cDataVec) {
        addNode(cData);
      }
    }

    unsigned int getNumTypeVersions() {
      return typeVersions.size();
    }

    bool init() {
      if (typeVersions.size() > 1) {
        solutionFound = solve();
        // std::cout << " ==== ==== SOLUTION ROWS ==== ==== " << std::endl;
        // for (auto & tv : typeVersions) {
        //   std::cout << "{ ";
        //   for (auto & idx : finalSolution) {
        //     tv[idx].print(std::cout);
        //     std::cout << "; ";
        //   }
        //   std::cout << "}" << std::endl;
        // }
        // std::cout << "===================================" << std::endl;
        // assert (checkValidity(finalSolution));
        if (!checkValidity(finalSolution)) {
          return false;
        }

        auto solSize = finalSolution.size();

        if (solSize > MAX_SLOTS_SIZE) {
          MAX_SLOTS_SIZE = solSize;
        }

        #if DEBUG_IMP_CHECKPOINTS == 1
        std::cout << "    Type Slots(" << finalSolution.size() << "): [ ";
        for (auto & ele : finalSolution) {
          std::cout << ele << " ";
        }
        std::cout << "]" << std::endl;

        std::cout << " ==== ==== SOLUTION ROWS ==== ==== " << std::endl;
        for (auto & tv : typeVersions) {
          std::cout << "{ ";
          for (auto & idx : finalSolution) {
            tv[idx].print(std::cout);
            std::cout << "; ";
          }
          std::cout << "}" << std::endl;
        }
        std::cout << "===================================" << std::endl;
        #endif
      } else {
        solutionFound = true;
      } 
       
      return solutionFound;
    }

    unsigned int getBinariesCount() {
      unsigned int res = 0;
      for (unsigned int i = 0; i < typeVersions.size(); i++) {
        res += nodes[i].get().size();
      }

      return res;
    }
    
    void print(unsigned int space = 0) {
      assert(typeVersions.size() == nodes.size());
      // printSpace(space);
      // std::cout << "Found " << typeVersions.size() << " Type Versions" << std::endl;
      for (unsigned int i = 0; i < typeVersions.size(); i++) {
        printSpace(space);
        std::cout << "├─(Type version " << i << "): " << nodes[i].get().size() << " binaries [ ";

        for (auto & ele : typeVersions[i]) {
          std::cout << getFeedbackAsUint(ele) << " ";
        }

        std::cout << "]" << std::endl;

        nodes[i].print(space + 2);
      }
      if (solutionFound) {
        if (typeVersions.size() > 1) {
          printSpace(space);
          std::cout << "└─(" << finalSolution.size() << " Slot Solution): ";
          for (auto & ele : finalSolution) {
            std::cout << ele << " ";
          }
          std::cout << std::endl;
        }

        // printSpace(space + 2);
        // std::cout << " ==== ==== SOLUTION ROWS ==== ==== " << std::endl;
        // for (auto & tv : typeVersions) {
        //   printSpace(space + 4);
        //   std::cout << "{ ";
        //   for (auto & idx : finalSolution) {
        //     tv[idx].print(std::cout);
        //     std::cout << "; ";
        //   }
        //   std::cout << "}" << std::endl;
        // }
        // printSpace(space + 2);
        // std::cout << "===================================" << std::endl;
      } else {
        Rf_error("No Slot solution found!");
      }


    }

    static void printStats(unsigned int space = 0) {
      printSpace(space);
      std::cout << "=== TVGraph Stats ===" << std::endl;
      
      printSpace(space);
      std::cout << "Max slots needed: " << MAX_SLOTS_SIZE << std::endl;
      
      printSpace(space);
      std::cout << "=====================" << std::endl;
    }

    void iterateOverTVs(const std::function< void(std::vector<uint32_t>, std::vector<SEXP>, TVNode) >& callback) {
      // std::cerr << "Iterate over TVs" << std::endl;

      rir::Protect protecc;
      static SEXP invalidSym = Rf_install("Invalid!");
      auto soln = getSolutionSorted();

      // std::cerr << " SOLUTION: [ ";
      // for (auto & ele : soln) {
      //   std::cerr << ele << " ";
      // }
      // std::cerr << "]" << std::endl;

      for (unsigned int i = 0; i < typeVersions.size(); i++) {
        auto currTV = typeVersions[i];

        auto node = nodes[i];

        // std::cerr << " FULL TV SLOTS (" << i << "): [ ";
        // for (auto & ele : currTV) {
        //   std::cerr << getFeedbackAsUint(ele) << " ";
        // }
        // std::cerr << "]" << std::endl;

        // std::cerr << "  node.getGeneralFeedbackLen(): " << node.getGeneralFeedbackLen() << std::endl;

        // std::cerr << "  node.getTypeFeedbackLen(): " << node.getTypeFeedbackLen() << std::endl;
        std::vector<SEXP> generalSlotData;
        std::vector<uint32_t> typeSlotData;
        for (auto & idx : soln) {
          if (idx < node.getTypeFeedbackLen()) {
            typeSlotData.push_back(getFeedbackAsUint(currTV[idx]));
          } else {
            auto currData = getFeedbackAsUint(currTV[idx]);
            // Restore adapter data
            if (currData == 0) {
              generalSlotData.push_back(R_NilValue);
              #if DEBUG_GENERAL_FEEDBACK_ADAPTER_OUT > 0
              std::cerr << "[ADAP OUT] NIL" << std::endl;
              #endif
            } else if (currData == 10) {
              generalSlotData.push_back(R_dot_defined);
              #if DEBUG_GENERAL_FEEDBACK_ADAPTER_OUT > 0
              std::cerr << "[ADAP OUT] T" << std::endl;
              #endif
            } else if (currData == 20) {
              generalSlotData.push_back(R_dot_Method);
              #if DEBUG_GENERAL_FEEDBACK_ADAPTER_OUT > 0
              std::cerr << "[ADAP OUT] F" << std::endl;
              #endif
            } else  {
              SEXP currSEXP = getIndirectionByIndex(currData);
              assert(currSEXP != invalidSym);
              assert(TYPEOF(currSEXP) == SYMSXP);

              std::string currStr(CHAR(PRINTNAME(currSEXP)));
              size_t start = currStr.find("_");
              assert(start != std::string::npos);

              SEXP hast = Rf_install(currStr.substr(0, start).c_str());
              SEXP index;

              protecc(index = Rf_ScalarInteger(std::stoi(currStr.substr(start+1).c_str())));

              #if DEBUG_GENERAL_FEEDBACK_ADAPTER_OUT > 0
              std::cerr << "ORIGINAL  : " << CHAR(PRINTNAME(currSEXP)) << std::endl;
              std::cerr << "PROCESSED : (" << CHAR(PRINTNAME(hast)) << "," << Rf_asInteger(index) << ") " << std::endl;
              #endif


              // TODO UPDATE TO OTHER
              SEXP store;

              protecc(store = Rf_allocVector(VECSXP, 2));
              SET_VECTOR_ELT(store, 0, hast);
              SET_VECTOR_ELT(store, 1, index);

              generalSlotData.push_back(store);
            }

          }
        }


        callback(typeSlotData, generalSlotData, nodes[i]);
      }
    }

    std::vector<int> getSolutionSorted() {
      assert(solutionFound == true);
      std::vector<int> res(finalSolution.begin(), finalSolution.end());
      std::sort(res.begin(), res.end());
      return res;
    }

    std::set<int> getSolution() {
      assert(solutionFound == true);
      return finalSolution;
    }

    unsigned int getSolutionSize() {
      assert(solutionFound == true);
      return finalSolution.size();
    }

    unsigned int getTotalNumberOfSlots() {
      assert(typeVersions.size() > 0);
      return typeVersions[0].size();
    }

    static SEXP getIndirectionByIndex(unsigned int idx) {
      for (auto & ele : feedbackIndirections) {
        if (ele.second == idx) {
          return ele.first;
        }
      }
      return Rf_install("Invalid!");
    }

    // 
    // Takes a contextData SEXP object and returns std::vector<rir::ObservedValues>
    // 
    static TFVector getFeedbackAsVector(SEXP cData) {
      TFVector res;

      SEXP tfContainer = rir::contextData::getTF(cData);
      rir::ObservedValues * tmp = (rir::ObservedValues *) DATAPTR(tfContainer);
      for (int i = 0; i < Rf_length(tfContainer) / (int) sizeof(rir::ObservedValues); i++) {
          res.push_back(tmp[i]);      
      }

      // Use simple indirection to do slot selection, can patch it back later
      SEXP otherFeedbackContainer = rir::contextData::getFBD(cData);
      for (int i = 0; i < Rf_length(otherFeedbackContainer); i++) {
        SEXP ele = VECTOR_ELT(otherFeedbackContainer, i);
        rir::ObservedValues nVal;

        uint32_t * v = (uint32_t *) &nVal;

        if (ele == R_NilValue) {
          *v = 0;
          #if DEBUG_GENERAL_FEEDBACK_ADAPTER_IN > 0
          // Check if we are correct
          std::cerr << "Actual: " << 0 << ", Stored: " << *((uint32_t *) &nVal) << std::endl;
          #endif
        } else if (ele == R_dot_defined) {
          *v = 10;
          #if DEBUG_GENERAL_FEEDBACK_ADAPTER_IN > 0
          // Check if we are correct
          std::cerr << "Actual: " << 10 << ", Stored: " << *((uint32_t *) &nVal) << std::endl;
          #endif
        } else if (ele == R_dot_Method) {
          *v = 20;
          #if DEBUG_GENERAL_FEEDBACK_ADAPTER_IN > 0
          // Check if we are correct
          std::cerr << "Actual: " << 20 << ", Stored: " << *((uint32_t *) &nVal) << std::endl;
          #endif
        } else if (TYPEOF(ele) == VECSXP){

          auto hast = VECTOR_ELT(ele, 0);
          auto index = Rf_asInteger(VECTOR_ELT(ele, 1));
          std::stringstream ss;
          ss << CHAR(PRINTNAME(hast)) << "_" << index;
          auto currKey = Rf_install(ss.str().c_str());

          // Key already seen
          if (feedbackIndirections.count(currKey) > 0) {
            *v = feedbackIndirections[currKey];
          } else {
            *v = indIdx;
            feedbackIndirections[currKey] = indIdx;
            indIdx++;
          }
          #if DEBUG_GENERAL_FEEDBACK_ADAPTER_IN > 0
          // Check if we are correct
          std::cerr << "Actual: " << ss.str() << ", Stored: " << CHAR(PRINTNAME(getIndirectionByIndex(*((uint32_t *) &nVal)))) << ", At idx: " << *((uint32_t *) &nVal) << std::endl;
          #endif
        } else {
          *v = 0;
          #if DEBUG_GENERAL_FEEDBACK_ADAPTER_IN > 0
          // Check if we are correct
          std::cerr << "Actual: " << 0 << ", Stored: " << *((uint32_t *) &nVal) << std::endl;
          #endif
        }
        static int skipGeneralFeedback = getenv("SKIP_GENERAL_FEEDBACK") ? std::stoi(getenv("SKIP_GENERAL_FEEDBACK")) : 0;

        if (skipGeneralFeedback) {
          *v = 0;
        }

        res.push_back(nVal);

      }

      return res;
    }

    // 
    // Takes a rir::ObservedValue and returns equivalent uint32_t
    // 
    static uint32_t getFeedbackAsUint(const rir::ObservedValues & v) {
      return *((uint32_t *) &v);
    }

    // 
    // Returns a set of indices where the two Type Feedback vectors differ at
    // We know that diff cannot possibly contain duplicates
    // 
    std::set<int> getDiffSet(std::pair<int, int> eles) {
      auto first = typeVersions[eles.first];
      auto second = typeVersions[eles.second];

      std::set<int> diffSet;

      assert(first.size() == second.size());
      for (unsigned int i = 0; i < first.size(); i++) {
        uint32_t v1 = getFeedbackAsUint(first[i]);
        uint32_t v2 = getFeedbackAsUint(second[i]);

        if (v1 != v2) {
          diffSet.insert(i);
        }
      }



      return diffSet;
    }

    static std::set<int> getDiffSet(TFVector first, TFVector second) {
      std::set<int> diffSet;

      assert(first.size() == second.size());
      for (unsigned int i = 0; i < first.size(); i++) {
        uint32_t v1 = getFeedbackAsUint(first[i]);
        uint32_t v2 = getFeedbackAsUint(second[i]);

        if (v1 != v2) {
          diffSet.insert(i);
        }
      }

      return diffSet;
    }


  private:    

    bool solutionFound = false;
    SolutionBucket finalSolution;
    std::vector<TFVector> typeVersions;
    std::unordered_map<unsigned int, TVNode> nodes;

    // std::unordered_map<std::string, bool> memo;

    // 
    // Helper methods
    // ====================================================================================
    // 

    

    // 
    // Takes a contextData SEXP and extracts its type feedback vector. If the type feedback 
    // vector already exists then adds the current node as a redundant node to it, otherwise
    // creates a new entry for the current node
    // 
    void addNode(std::pair<SEXP, SEXP> cData) {
      TFVector currTFVector = getFeedbackAsVector(cData.second);
      // CURB with blacklist to prevent infinite loops
      for (auto & ele : blacklist) {
        rir::ObservedValues rv;
        uint32_t* v = (uint32_t *) &rv;
        *v = 0;
        currTFVector[ele] = rv;
      }
      
      // If curr exists then add to an existing node
      unsigned int i = 0;
      for (; i < typeVersions.size(); i++) {
        if (TFVEquals(typeVersions[i], currTFVector)) {
          nodes[i].addNode(cData);
          return;
        }
      }

      // If this is a new version, add new
      typeVersions.push_back(currTFVector);
      size_t idx = typeVersions.size() - 1;
      nodes[idx].addNode(cData);
    }

    public:

    int getGeneralFeedbackLen() {
      return nodes[0].getGeneralFeedbackLen();
    }

    int getTypeFeedbackLen() {
      return nodes[0].getTypeFeedbackLen();
    }

    private:


    


    // 
    // Returns true if the given TFVectors are the same 
    // 
    bool TFVEquals(TFVector & v1, TFVector & v2) {
      assert(v1.size() == v2.size());

      for (unsigned int i = 0; i < v1.size(); i++) {
        uint32_t t1 = getFeedbackAsUint(v1[i]);
        uint32_t t2 = getFeedbackAsUint(v2[i]);
        if (t1 != t2) {
          return false;
        }
      }
      return true;
    }

    // 
    // For validating the results that the solver returns, takes a SolutionBucket(std::set<int>) and returns a bool
    // 
    bool checkValidity(SolutionBucket & indices, bool printDebug=false) {
      std::vector<std::vector<uint32_t>> res;

      for (auto & tv : typeVersions) {
        std::vector<uint32_t> ele;
        for (auto & idx : indices) {
          uint32_t curr = getFeedbackAsUint(tv[idx]);
          ele.push_back(curr);
        }
        res.push_back(ele);
      }

      // Check if duplicate elements exist
      for (unsigned int m = 0; m < res.size(); m++) {
        auto first = res[m];

        for (unsigned int i = 0; i < res.size(); i++) {
          if (i == m) continue;
          auto curr = res[i];

          bool match = true;

          for (unsigned int j = 0; j < first.size(); j++) {
            if (first[j] != curr[j]) {
              match = false;
            }
          }

          
          // If match is true, then return false as we cannot uniquely identify using this indices subset
          if (match) {
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

    // 
    // Better Slot Finder implementation
    // ====================================================================================
    // 

    // 
    // Solver Handle
    // 
    bool solve() {
      Worklist genesis = getGenesisWorklist();
      SolutionBucket genesisS;

      auto tSol = solveTrivialCases(genesis, genesisS);

      if (tSol.first.size() == 0) {
        finalSolution = tSol.second;
        if (tSol.second.size() < SLOT_FINDER_BUDGET) {
          #if DEBUG_BACKTRACKING_SOLVER == 1
          std::cout << "        (*) Found solution trivially!" << std::endl;
          #endif
          return true;
        } else {
          #if DEBUG_BACKTRACKING_SOLVER == 1
          std::cout << "        (*) Solution found but no solution in allocated budget exists!" << std::endl;
          #endif
          return false;
        }
      }

      #if DEBUG_BACKTRACKING_SOLVER == 1
      std::cout << "        Genesis Worklist: [ ";
      for (auto & e : tSol.first) {
        std::cout << "(" << e.first << "," << e.second << ") ";
      }
      std::cout << "]" << std::endl;

      std::cout << "        Genesis Solution: [ ";
      for (auto & ele : tSol.second) {
        std::cout << ele << " ";
      }
      std::cout << "]" << std::endl;

      #endif

      return backtrackingSolver(tSol.first, tSol.second);
    }

    // 
    // Recusive solving routine, recurse only if improvement exists otherwise backtrack
    // 
    bool backtrackingSolver(Worklist currWorklist, SolutionBucket currSol) {
      std::stringstream key;
      for (auto & e : currWorklist) {
        key << e.first << "," << e.second << ",";
      }
      key << "_";
      for (auto & e : currSol) {
        key << e;
      }

      // if (memo.find(key.str()) != memo.end()) {
      //   return memo[key.str()];
      // }

      if (currSol.size() > SLOT_FINDER_BUDGET) {
        // memo[key.str()] = false;
        return false;
      }

      // Reduce worklist
      auto so = solveTrivialCases(currWorklist, currSol);
      currWorklist = so.first;
      currSol = so.second;
      
      if (currWorklist.size() == 0) {
        finalSolution = currSol;
        // memo[key.str()] = true;
        return true;
      }

      #if DEBUG_BACKTRACKING_SOLVER > 0
      std::cout << "        CURRENT_WORKLIST: [ ";
      for (auto & e : currWorklist) {
        std::cout << "(" << e.first << "," << e.second << ") ";
      }
      std::cout << "]" << std::endl;

      std::cout << "        CURRENT_SOLUTION: [ ";
      for (auto & ele : currSol) {
        std::cout << ele << " ";
      }
      std::cout << "]" << std::endl;
      #endif

      // SMART STAGE
      for (WorklistElement & ele : currWorklist) {
        // Our aim is to minimize the diffSet.
        // For a given worklist element, diffset gives us a list of possible solutions
        // we want to only try the solutions that will actually improve things

        SolutionBucket diffSet = getDiffSet(ele);

        #if DEBUG_BACKTRACKING_SOLVER > 1
        std::cout << "          NEXT_DIFFSET_STAGE_0: [ ";
        for (auto & ele : diffSet) {
          std::cout << ele << " ";
        }
        std::cout << "]" << std::endl;
        #endif

        // 1. Remove existing solutions from potential solutions
        for (auto & s : currSol) {
          if (diffSet.find(s) != diffSet.end()) {
            diffSet.erase(s);
          }
        }

        #if DEBUG_BACKTRACKING_SOLVER > 1
        std::cout << "          NEXT_DIFFSET_STAGE_1: [ ";
        for (auto & ele : diffSet) {
          std::cout << ele << " ";
        }
        std::cout << "]" << std::endl;
        #endif

        SolutionBucket considerationSubsetCols;
        // 2. Get rid of functionally useless solutions
        for (auto & fele : diffSet) {
          SolutionBucket tempSolBucket(currSol.begin(), currSol.end());
          
          // Add this potential solution to the existing solution set
          tempSolBucket.insert(fele);

          auto soln = solveTrivialCases(currWorklist, tempSolBucket);
          
          // Worklist empty, If yes check and return final res
          if (soln.first.size() == 0) {
            finalSolution = tempSolBucket;
            
            
            auto res = backtrackingSolver(soln.first, soln.second);
            // memo[key.str()] = res;
            return res;
          }
          // If the worklist with the newly added solution is smaller than the existing worklist, we keep it
          if (soln.first.size() < currWorklist.size()) {
            considerationSubsetCols.insert(fele);
          }
        }

        #if DEBUG_BACKTRACKING_SOLVER > 1
        std::cout << "          NEXT_DIFFSET_STAGE_2: [ ";
        for (auto & ele : considerationSubsetCols) {
          std::cout << ele << " ";
        }
        std::cout << "]" << std::endl;
        #endif

        // // TODO: Fix this reduction stage in future if needed, other stages handles all cases effortlessly already.
        // // 2. Eliminate functionally identical solutions
        // SolutionBucket finalConsiderationBucket = removeFunctionallyEquivalentSols(currWorklist, considerationSubsetCols);


        // #if DEBUG_BACKTRACKING_SOLVER > 1
        // std::cout << "          NEXT_DIFFSET_STAGE_3: [ ";
        // for (auto & ele : finalConsiderationBucket) {
        //   std::cout << ele << " ";
        // }
        // std::cout << "]" << std::endl;
        // #endif
        
        // 3. Iterate over all combinations of the reduced solution set
        std::vector<int> ints(considerationSubsetCols.begin(), considerationSubsetCols.end());

        for (unsigned int j = 1; j < ints.size(); j ++) {
          if ((j + currSol.size()) > SLOT_FINDER_BUDGET) break;

          CombinationsIndexArray combos(ints.size(), j);
          do {
            SolutionBucket tempSolBucket(currSol.begin(), currSol.end());
            for (int i = 0; i < combos.size(); i++) {
              tempSolBucket.insert(ints[combos[i]]);
            }

            #if DEBUG_BACKTRACKING_SOLVER > 1
            std::cout << "            NEXT_FINAL_SOLSET: [ ";
            for (auto & ele : considerationSubsetCols) {
              std::cout << ele << " ";
            }
            std::cout << "]" << std::endl;
            #endif


            bool res = backtrackingSolver(currWorklist, tempSolBucket);
            // memo[key.str()] = res;
            if (res) {
              return true;
            }

          } while (combos.advance());

        }


      }

      return false;

    }

    //
    // If the solution set contains any of the diff-set element, then it is already solved
    // 
    bool checkIfEleAlreadySolved(WorklistElement & ele, SolutionBucket & bucket) {
      std::set<int> diffSet = getDiffSet(ele);
      for (auto & s : bucket) {
        for (auto & d : diffSet) {
          if (s == d) return true;
        }
      }

      return false;
    }

    //
    // Solves the trivial cases, reduces worklist if either condition is met
    //  1: Diffset of ele already belongs to the solutionBucket
    //  2: The diff set size is one
    // and returns a new worklist and solutionBucket
    // 

    std::pair<Worklist, SolutionBucket> solveTrivialCases(Worklist existingWorklist, SolutionBucket existingSolutionBucket) {
      Worklist newWorklist;
      SolutionBucket newSolutionBucket(existingSolutionBucket.begin(), existingSolutionBucket.end());
      Worklist tempWorklist;

      for (WorklistElement & ele : existingWorklist) {
        auto diffSet = getDiffSet(ele);
        if (diffSet.size() == 1) {
          auto first = *diffSet.begin();
          newSolutionBucket.insert(first);
        } else {
          tempWorklist.push_back(ele);
        }
      }

      for (WorklistElement & ele : tempWorklist) {
        if (checkIfEleAlreadySolved(ele, newSolutionBucket) == false) {
          newWorklist.push_back(ele);
        }
      }

      return std::pair<Worklist, SolutionBucket>(newWorklist, newSolutionBucket);
    }


    // 
    // Returns the genesis worklist, it returns n choose 2 over the list of Type Feedback versions
    // We expect this to be efficient enough as even something like 100 type versions has a manageable
    // result of 4952
    // 
    Worklist getGenesisWorklist() {
      Worklist res;

      // N choose 2
      CombinationsIndexArray combos(typeVersions.size(), 2);
      do {
        std::vector<int> indices;
        for (int i = 0; i < combos.size(); i++) {
          indices.push_back(combos[i]);
        }

        assert(indices.size() == 2);
        res.push_back(std::pair<int, int>(indices[0], indices[1]));

      } while (combos.advance());

      return res;

    }

    // 
    // // TODO -- currently broken, will be fixed if need arises
    // If two different solutions when applied to a worklist return the same solution, we consider them
    // functionally equivalent
    // SolutionBucket --> set<ints>
    // Worklist --> std::vector<std::pair<int, int>>
    // 
    // SolutionBucket removeFunctionallyEquivalentSols(Worklist wl, SolutionBucket & considerationBucket) {
    //   SolutionBucket newBucket;

    //   std::vector<int> candidateSolutions(considerationBucket.begin(), considerationBucket.end());

    //   std::vector<int> toRemove;

    //   for (int i = 0; i < candidateSolutions.size(); i++) {
    //     // Skip removed indices
    //     if (std::find(toRemove.begin(), toRemove.end(), i) != toRemove.end()) {
    //       continue;
    //     }

    //     newBucket.insert(candidateSolutions[i]);

    //     for (int j = i + 1; j < candidateSolutions.size(); j++) {


    //       SolutionBucket tempSolBucket1(considerationBucket.begin(), considerationBucket.end());
    //       tempSolBucket1.insert(candidateSolutions[i]);

    //       SolutionBucket tempSolBucket2(considerationBucket.begin(), considerationBucket.end());
    //       tempSolBucket2.insert(candidateSolutions[j]);


    //       auto soln1 = solveTrivialCases(wl, tempSolBucket1);
    //       auto soln2 = solveTrivialCases(wl, tempSolBucket2);

    //       // If both lead to same worklist then only keep the first one
    //       if (soln1 == soln2) {
    //         std::cout << "      (*) Found same worklist, removing redundant soln idx" << std::endl;
    //         toRemove.push_back(j);
    //       }

    //     }
    //   }

    //   std::cout << "      (*) [Functional reduction] Before: " << considerationBucket.size() << ", After: " << newBucket.size() << std::endl;

    //   return newBucket;
    // }

};