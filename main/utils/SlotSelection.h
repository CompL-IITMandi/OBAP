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
        if (sol.count(ele) == 0) { // Prevent addition of already added solutions
          res.insert(ele);
        }
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
    std::vector<Reducer> sol;
    std::vector<size_t> res;
    for (auto & s : possibleSolutions) {
      std::set<size_t> tSol;
      tSol.insert(s);
      auto tWl = reduceWorklist(wl, tSol);
      sol.push_back({s, tWl.size()});
    }
    std::sort(sol.begin(), sol.end(), reducerSortPredicate);        
    for (auto & ele : sol) {
      res.push_back(ele.solIdx);
    }
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

  void findSolution(SlotSelectionSolution * solnHolder = nullptr, bool recurse = false) {
    assert(solve(_worklist, _solution));
    for (auto & ele : _nodes) {
      if (ele.second.size() == 1) {
        solnHolder->addSolution(_criteria, ele.second, _fsolution);
      } else if (recurse) {
        std::set<SEXP> H = getSpeculativeContextUnion(ele.second);
        std::unordered_map<SEXP, std::vector<SBinary>> recurseMap;
        for (auto & hast : H) {
          for (auto & binary : ele.second) {
            if (binary.getSpeculativeContext().containsVector(hast)) {
              recurseMap[hast].push_back(binary);
            }
          }
        }
        for (auto & ele : recurseMap) {
          if (used.count(ele.first) > 0) continue;
          if (ele.second.size() > 1) {
            SlotSelectionGraph ssg(ele.first);
            ssg.addBinaries(ele.second);
            ssg.init();
            ssg.findSolution(solnHolder, false);
          }
        }
      }
    }
  }

  bool solve(std::vector<std::pair<FeedbackVector, FeedbackVector>> wl, std::set<size_t> sol) {
    if (sol.size() > BUDGET) return false;
    if (wl.size() == 0) {
      if (sol.size() <= BUDGET) {
        _fsolution = sol;
        return true;
      }
      return false;
    }

    if (sol.size() + 1 > BUDGET) return false;
    std::set<size_t> possibleSolutions = getPossibleSolutions(wl, sol); // n
    std::vector<size_t> sortedSolutions = getSortedByReduction(possibleSolutions, wl); // nlogn
    std::set<size_t> nextSol(sol.begin(), sol.end()); // b + nlogn
    nextSol.insert(sortedSolutions[0]); // Highest number of reductions
    std::vector<std::pair<FeedbackVector, FeedbackVector>> resWl = reduceWorklist(wl, nextSol); // b + nlogn
    if (solve(resWl, nextSol)) return true;
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