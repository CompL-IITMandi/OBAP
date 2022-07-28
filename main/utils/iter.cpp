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

#define DEBUG_PRINT_GENESIS 0

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
    bool checkValidity(std::set<int> & indices, bool printDebug=false) {
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

    // std::pair<bool, std::vector<int>> findSlotIn(size_t k) {
    //   std::vector<unsigned int> ints = getDiffSlots();
    //   std::sort(ints.begin(), ints.end(), 
    //       [&](const unsigned int & idx1, const unsigned int & idx2) {
    //         std::set<unsigned int> s1, s2;
    //         for (auto & currVer: typeVersions) {
    //           uint32_t c1 = *((uint32_t *) &currVer[idx1]);
    //           uint32_t c2 = *((uint32_t *) &currVer[idx2]);
    //           s1.insert(c1);
    //           s2.insert(c2);
    //         }
    //         return s1.size() < s2.size();
    //       });

    //   std::cout << "      [SORTED] Slots in focus(" << ints.size() << "): [ ";
    //   for (auto & ele : ints) {
    //     std::cout << ele << " ";
    //   }
    //   std::cout << "]" << std::endl;


    //   if (k == 1) {
    //     for (auto & ele : ints) {
    //       // std::cout << "Trying (" << ele << ")" << std::endl;
    //       std::vector<int> indices;
    //       indices.push_back(ele);
    //       bool res = checkValidity(indices);
    //       if (res) {
    //         return std::pair<bool, std::vector<int>>(true, res);
    //       }  
    //     }

    //     return std::pair<bool, std::vector<int>>(false, std::vector<int>());
    //   }

    //   if (ints.size() < k) {
    //     std::cout << "Early Quit for " << k << std::endl;
    //     return std::pair<bool, std::vector<int>>(false, std::vector<int>());
    //   }

    //   auto totalPossibilities = nChoosek(ints.size(), k);
    //   std::cout << "  TOTAL POSSIBILITIES = " << totalPossibilities << std::endl;
    //   bool TPM = true;
    //   int count = 0;

    //   Ticker tickTick(totalPossibilities);

    //   // std::vector<std::thread> threadPool;

    //   bool solnFound = false;
    //   std::vector<int> solnRes;

    //   std::vector<std::thread> threadPool;
    //   threadPool.reserve(MAX_POOL_SIZE);
      // CombinationsIndexArray combos(ints.size(), k);
      // do {
        
      //   std::vector<int> indices;
      //   for (int i = 0; i < combos.size(); i++) {
      //     indices.push_back(ints[combos[i]]);
      //   }

      //   if (threadPool.size() > MAX_POOL_SIZE) {
      //     for (auto & t : threadPool) {
      //       t.join();
      //     }
      //     threadPool.clear();
      //   }

      //   threadPool.emplace_back([&](std::vector<int> idi) {
      //     count++;
      //     bool res = checkValidity(idi);
      //     if (res) {
      //       solnFound = true;
      //       solnRes = idi;
      //     }
      //   }, indices);

      //   if (count > TICK_TICK_LAP_COUNT) {
      //     tickTick.lap(count);
      //     count=0;
      //   }

      //   if (solnFound) {
      //     break;
      //   }
      // } while (combos.advance());

    //   for (auto & t: threadPool) {
    //     t.join();
    //   }

    //   if (solnFound) {
    //     return std::pair<bool, std::vector<int>>(true, solnRes);
    //   }
    //   return std::pair<bool, std::vector<int>>(false, std::vector<int>());
    // }

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
      if (typeVersions.size() > 1) {
        if (solve()) {
          std::cout << "        (S) Solution: [ ";
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
          assert (checkValidity(finalSolution));
        } else {
          std::cout << "        (S) No solution found in the allocated budget!" << std::endl;
        }


        // for (int i = 1; i <= MAX_SLOTS; i++) {
        //   std::cout << "    Looking for " << i << " slot solution" << std::endl;
        //   auto result = findSlotIn(i);
        //   if (result.first) {
        //     std::cout << "        Found result in " << i << " slots" << std::endl;
        //     checkValidity(result.second, true);
        //     return;
        //   }
        //   std::cout << "        No result in " << i << " slots" << std::endl;
        // }
        // std::cout << "        No Result found (Tried until " << MAX_SLOTS << ")" << std::endl;
      }
    }

    

  private:


    typedef std::pair<int, int> WorklistElement;
    typedef std::vector<std::pair<int, int>> Worklist;
    typedef std::set<int> SolutionBucket;

    int THRESHOLD_BUDGET = 10;
    SolutionBucket finalSolution;

    // 
    // Solver Handle
    // 
    bool solve() {

      Worklist genesis = getGenesisWorklist();
      SolutionBucket genesisS;

     

      auto tSol = solveTrivialCases(genesis, genesisS);

      if (tSol.first.size() == 0) {
        finalSolution = tSol.second;
        if (tSol.second.size() < THRESHOLD_BUDGET) {
          std::cout << "        (*) Found solution trivially!" << std::endl;
          return true;
        } else {
          std::cout << "        (*) Solution found but no solution in allocated budget exists!" << std::endl;
          return false;
        }
      }

      #if DEBUG_PRINT_GENESIS == 1
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
    // If two different solutions when applied to a worklist return the same solution, we consider them
    // functionally equivalent
    // 
    SolutionBucket removeFunctionallyEquivalentSols(Worklist wl, SolutionBucket & considerationBucket) {
      std::vector<Worklist> solutions;
      SolutionBucket newBucket;

      for (auto & s : considerationBucket) {
        SolutionBucket tempSolBucket(considerationBucket.begin(), considerationBucket.end());
        tempSolBucket.insert(s);
        auto soln = solveTrivialCases(wl, tempSolBucket);

        bool differentWorklist = false;

        // Check if any of the previous solution matches with the current solution
        for (auto & wl : solutions) {
          // If they are same, they will have the same size aswell
          if (wl.size() == soln.first.size()) {
            for (int i = 0; i < wl.size(); i++) {
              auto first = wl[i];
              auto second = soln.first[i];
              // If even one element is different they they are different
              if (first.first != second.first || first.second != second.second) {
                differentWorklist = true;break;
              }
            }
          }
          if (differentWorklist) {
            break;
          }
        }

        // If this solution may lead to a different worklist, then include it
        if (differentWorklist) {
          newBucket.insert(s);
        }

      }

      return newBucket;
    }

    // 
    // Recusive solving routine, recurse only if improvement exists otherwise backtrack
    // 
    bool backtrackingSolver(Worklist currWorklist, SolutionBucket currSol) {
      if (currSol.size() > THRESHOLD_BUDGET) {
        return false;
      }

      // Reduce worklist
      auto so = solveTrivialCases(currWorklist, currSol);
      currWorklist = so.first;
      currSol = so.second;
      
      if (currWorklist.size() == 0) {
        std::cout << "Solution Found" << std::endl;
        finalSolution = currSol;
        return true;
      }

      #if DEBUG_PRINT_GENESIS == 1
      std::cout << "        BTWL: [ ";
      for (auto & e : currWorklist) {
        std::cout << "(" << e.first << "," << e.second << ") ";
      }
      std::cout << "]" << std::endl;

      std::cout << "        CSOL: [ ";
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

        // #if DEBUG_PRINT_GENESIS == 1
        // std::cout << "          DIFFSET   : [ ";
        //     for (auto & ele : diffSet) {
        //       std::cout << ele << " ";
        //     }
        //     std::cout << "]" << std::endl;
        // #endif

        // 1. Remove existing solutions from potential solutions
        for (auto & s : currSol) {
          if (diffSet.find(s) != diffSet.end()) {
            diffSet.erase(s);
          }
        }

        // #if DEBUG_PRINT_GENESIS == 1
        // std::cout << "          DIFFSET(R): [ ";
        //     for (auto & ele : diffSet) {
        //       std::cout << ele << " ";
        //     }
        //     std::cout << "]" << std::endl;
        // #endif

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
            return backtrackingSolver(soln.first, soln.second);
          }
          // If the worklist with the newly added solution is smaller than the existing worklist, we keep it
          if (soln.first.size() < currWorklist.size()) {
            considerationSubsetCols.insert(fele);
          }
        }

        // #if DEBUG_PRINT_GENESIS == 1
        // std::cout << "          CONSIDE  : [ ";
        //     for (auto & ele : considerationSubsetCols) {
        //       std::cout << ele << " ";
        //     }
        //     std::cout << "]" << std::endl;
        // #endif

        // // 2. Eliminate functionally identical solutions
        // SolutionBucket finalConsiderationBucket = removeFunctionallyEquivalentSols(currWorklist, considerationSubsetCols);
        
        // 3. Iterate over all combinations of the reduced solution set
        std::vector<int> ints(considerationSubsetCols.begin(), considerationSubsetCols.end());

        for (int j = 1; j < ints.size(); j ++) {
          if ((j + currSol.size()) > THRESHOLD_BUDGET) break;

          CombinationsIndexArray combos(ints.size(), j);
          do {
            SolutionBucket tempSolBucket(currSol.begin(), currSol.end());
            for (int i = 0; i < combos.size(); i++) {
              tempSolBucket.insert(ints[combos[i]]);
            }

            #if DEBUG_PRINT_GENESIS == 1
            std::cout << "            CONSIDE(R): [ ";
            for (auto & ele : tempSolBucket) {
              std::cout << ele << " ";
            }
            std::cout << "]" << std::endl << std::endl;
            #endif


            bool res = backtrackingSolver(currWorklist, tempSolBucket);

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
    // Returns a set of indices where the two Type Feedback vectors differ at
    // We know that diff cannot possibly contain duplicates
    // 
    std::set<int> getDiffSet(std::pair<int, int> eles) {
      auto first = typeVersions[eles.first];
      auto second = typeVersions[eles.second];

      std::set<int> diffSet;

      assert(first.size() == second.size());
      for (int i = 0; i < first.size(); i++) {
        uint32_t v1 = *((uint32_t *)(&first[i]));
        uint32_t v2 = *((uint32_t *)(&second[i]));

        if (v1 != v2) {
          diffSet.insert(i);
        }
      }

      return diffSet;
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