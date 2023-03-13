#pragma once
#include "Rinternals.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <set>
#include "utils/Debug.h"
#include "utils/SerializerAbstraction.h"
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

class SlotSelectionGraph {
public:
  static std::set<SEXP> used;

  SlotSelectionGraph(SEXP criteria) : _criteria(criteria) { }

  void addBinaries(std::vector<SBinary> bins) {
    for (auto & bin : bins) {
      auto fv = bin.getSpeculativeContext().getVector(_criteria);
      _nodes[fv].push_back(bin);
    }
  }

  bool init() {
    // Initialize worklist
    std::vector<FeedbackVector> v;
    for (auto & ele : _nodes) {
      v.push_back(ele.first);
    }

    for (size_t i = 0; i < v.size() - 1; i++) {
      for (size_t j = i+1; j < v.size(); j++) {
        _worklist.push_back(std::pair<FeedbackVector,FeedbackVector>(v[i],v[j]));
      }
    }

    // Reduce using trivial solutions
    auto triv = getTrivialSolutions();

    // int space = 2;
    // printSpace(space + 2, std::cout);
    // std::cout << "╠═([init]worklist=" << _worklist.size() << ")" << std::endl;
    // for (auto & ele : _worklist) {
    //   printSpace(space + 4, std::cout);
    //   std::cout << "(" << ele.first.getUID() << "," << ele.second.getUID() << ")" << std::endl;
    // }
    // printSpace(space + 2, std::cout);
    // std::cout << "╠═([triv]worklist=" << triv.second.size() << ")" << std::endl;
    // for (auto & ele : triv.second) {
    //   printSpace(space + 4, std::cout);
    //   std::cout << "(" << ele.first.getUID() << "," << ele.second.getUID() << ")" << std::endl;
    // }
    // printSpace(space + 4, std::cout);
    // std::cout << "╠═(solution=" << triv.first.size() << "): [ ";
    // for (auto & ele : triv.first) {
    //   std::cout << ele << " ";
    // }
    // std::cout << " ]" << std::endl;

    _solution = triv.first;
    _worklist = triv.second;
    if (_solution.size() > BUDGET) return false;

    return true;
  }

  std::set<size_t> getPossibleSolutions(const std::vector<std::pair<FeedbackVector, FeedbackVector>> & wl, std::set<size_t> sol) {
    std::set<size_t> res;
    for (auto & w : wl) {
      auto currDiff = FeedbackVector::getDiff(w.first, w.second);
      assert(currDiff.size() > 0);
      for (auto & ele : currDiff) {
        res.insert(ele);
      }
    }
    return res;
  }

  struct Reducer {
    size_t solIdx;
    size_t newWl;
  };

  static bool reducerSortPredicate(const Reducer &a, const Reducer &b) {
    return a.newWl < b.newWl;
  }

  std::vector<size_t> getSortedByReduction(std::set<size_t> possibleSolutions, const std::vector<std::pair<FeedbackVector, FeedbackVector>> & wl) {
    // int space = 0;
    // printSpace(space + 4, std::cout);
    // std::cout << "╠═(OriginalList): [ ";
    // for (auto & ele : possibleSolutions) {
    //   std::cout << ele << " ";
    // }
    // std::cout << "]" << std::endl;

    std::vector<Reducer> sol;
    std::vector<size_t> res;
    for (auto & s : possibleSolutions) {
      std::set<size_t> tSol;
      tSol.insert(s);
      auto tWl = reduceWorklist(wl, tSol);
      sol.push_back({s, tWl.size()});
    }
    std::sort(sol.begin(), sol.end(), reducerSortPredicate);
    
    // printSpace(space + 4, std::cout);
    // std::cout << "╠═(Sorted): [ ";
    // for (auto & s : sol) {
    //   std::cout << "<" << s.solIdx << "," << s.newWl << "> ";
    // }
    // std::cout << "]" << std::endl;
    
    for (auto & ele : sol) {
      res.push_back(ele.solIdx);
    }

    // printSpace(space + 4, std::cout);
    // std::cout << "╠═(Res): [ ";
    // for (auto & r : res) {
    //   std::cout << r << " ";
    // }
    // std::cout << "]" << std::endl;
    return res;
  }

  std::set<SEXP> getSpeculativeContextUnion(std::vector<SBinary> v) {
    std::set<SEXP> res;
    for (auto & bin : v) {
      for (auto & hast : bin.getSpeculativeContext().getHasts()) {
        res.insert(hast);
      }
    }

    return res;
  }

  void findSolution(SlotSelectionSolution * solnHolder = nullptr) {
    used.insert(_criteria);
    assert(solve(_worklist, _solution));

    // std::vector<std::set<SBinary, std::set<size_t>>> res;

    for (auto & ele : _nodes) {
      if (ele.second.size() == 1) {
        solnHolder->addSolution(_criteria, ele.second, _fsolution);
        // _finalSolution.push_back(std::set<SBinary, std::set<size_t>>(ele.second[0], _fsolution));
      } else {
        // printSpace(2);
        // std::cout << "solving recursively" << std::endl;
        std::set<SEXP> H = getSpeculativeContextUnion(ele.second);
        std::unordered_map<SEXP, std::vector<SBinary>> recurseMap;
        for (auto & hast : H) {
          for (auto & binary : ele.second) {
            if (binary.getSpeculativeContext().containsVector(hast)) {
              recurseMap[hast].push_back(binary);
            }
          }
        }

        // std::unordered_map<SBinary, std::set<size_t>> finSol;
        // printSpace(2);
        // std::cout << "Working with recurse map" << std::endl;
        for (auto & ele : recurseMap) {
          if (used.count(ele.first) > 0) continue;
          if (ele.second.size() > 1) {
            SlotSelectionGraph ssg(ele.first);
            ssg.addBinaries(ele.second);
            ssg.init();
            // ssg.print(std::cout, 4);
            ssg.findSolution(solnHolder);
          }
        }

      }
      
    }

    used.erase(_criteria);

    // return res;
  }

  bool solve(std::vector<std::pair<FeedbackVector, FeedbackVector>> wl, std::set<size_t> sol) {
    if (sol.size() > BUDGET) return false;
    if (wl.size() == 0) {
      if (sol.size() <= BUDGET) {
        _fsolution = sol;
        // std::cout << "Found Solution(criteria=" << CHAR(PRINTNAME(_criteria)) << "): [ ";
        // for (auto & s : _fsolution) {
        //   std::cout << s << " ";
        // }
        // std::cout << "]" << std::endl;
        
        return true;
      }
      return false;
    }
    std::set<size_t> possibleSolutions = getPossibleSolutions(wl, sol);
    std::vector<size_t> sortedSolutions = getSortedByReduction(possibleSolutions, wl);
    for (unsigned int j = 1; j < sortedSolutions.size(); j ++) {
      if ((j + sol.size()) > BUDGET) return false;
      // Iterate over combinations of size j
      CombinationsIndexArray combos(sortedSolutions.size(), j);
      do {
        // union of existing solution set with to-try solution set
        std::set<size_t> nextSol(sol.begin(), sol.end());
        for (int i = 0; i < combos.size(); i++) {
          nextSol.insert(sortedSolutions[combos[i]]);
        }

        std::vector<std::pair<FeedbackVector, FeedbackVector>> resWl = reduceWorklist(wl, nextSol);
        if (solve(resWl, nextSol)) return true;

      } while (combos.advance());

    }
    return false;
  }

  std::vector<std::pair<FeedbackVector, FeedbackVector>> reduceWorklist(std::vector<std::pair<FeedbackVector, FeedbackVector>> wl, std::set<size_t> sol) {
    std::vector<std::pair<FeedbackVector, FeedbackVector>> res;
    for (auto & w : _worklist) {
      auto currDiff = FeedbackVector::getDiff(w.first, w.second);
      bool reduced = false;
      for (auto & s : sol) {
        if (currDiff.count(s)) {
          reduced = true;
          break;
        }
      }
      if (!reduced) {
        res.push_back(w);
      }
    }
    return res;
  }

  std::pair<std::set<size_t>, std::vector<std::pair<FeedbackVector, FeedbackVector>>> getTrivialSolutions() {
    std::set<size_t> resSol;
    for (auto & w : _worklist) {
      auto currDiff = FeedbackVector::getDiff(w.first, w.second);
      assert(currDiff.size() > 0);
      if (currDiff.size() == 1) {
        resSol.insert(*currDiff.begin());
      }
    }

    std::vector<std::pair<FeedbackVector, FeedbackVector>> resWl = reduceWorklist(_worklist, resSol);
    
    return std::pair<std::set<size_t>, std::vector<std::pair<FeedbackVector, FeedbackVector>>>(
      resSol,
      resWl
    );
  }

  void print(std::ostream& out, const int & space=0) {
    printSpace(space, out);
    out << "█═════════(criteria=" << CHAR(PRINTNAME(_criteria)) << ")═════════█" << std::endl;
    
    for (auto & ele : _nodes) {
      printSpace(space + 2, out);
      out << "╠═(Node=" << ele.first.getUID() << ",binaries=" << ele.second.size() << ")" << std::endl;
      // int i = 1;
      // for (auto & bin : ele.second) {
      //   printSpace(space + 4, out);
      //   out << "█═(binary=" << i++ << ")" << std::endl;
      //   bin.print(out, space + 6);
      // }
    }

    // printSpace(space + 2, out);
    // out << "╠═(worklist=" << _worklist.size() << ")" << std::endl;
    // for (auto & ele : _worklist) {
    //   printSpace(space + 4, out);
    //   out << "(" << ele.first.getUID() << "," << ele.second.getUID() << ")" << std::endl;
    // }
  }
  static size_t BUDGET;
private:
  SEXP _criteria;
  std::unordered_map<FeedbackVector, std::vector<SBinary>,FeedbackVectorHash> _nodes;
  std::vector<std::pair<FeedbackVector, FeedbackVector>> _worklist;
  std::set<size_t> _solution, _fsolution;
};