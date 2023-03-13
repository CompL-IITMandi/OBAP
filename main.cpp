#include <fstream>
// Include fstream before Rinternals.h, otherwise a macro redef error might happen
#include "Rinternals.h"

#include "Rembedded.h"
#include "dirent.h"
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>


#include "utils/serializerData.h"
#include "utils/Debug.h"
// #include "utils/SerializedDataProcessor.h"
#include "utils/RshBuiltinsMap.h"
#include "utils/UMap.h"
#include "utils/deserializerData.h"

#include <iostream>
#include <stdlib.h>
#include <functional>
#include <sstream>
#include <algorithm>

#include "R/Protect.h"

std::string outputPath;
std::string inputPath;

static int obapDEBUG = getenv("OBAP_DBG") ? std::stoi(getenv("OBAP_DBG")) : 0;

// static void saveDMetaAndCopyFiles(SEXP ddContainer, const std::string & metaFilename) {
//   std::stringstream outFilePath;

//   outFilePath << outputPath << "/" << metaFilename << "d";

//   FILE *fptr;
//   fptr = fopen(outFilePath.str().c_str(),"w");
//   if (!fptr) {
//     for (int i = 0; i < 10; i++) {
//       sleep(1);
//       std::cout << "[W]waiting to open: " << outFilePath.str() << std::endl;
//       fptr = fopen(outFilePath.str().c_str(),"w");
//       if (fptr) break;
//     }

//     if (!fptr) {
//       std::cout << "[W]unable to open " << outFilePath.str() << std::endl;
//       Rf_error("[W]unable to open file!");
//     }
//   }
  
//   R_SaveToFile(ddContainer, fptr, 0);
//   fclose(fptr);

//   // Copy all relavant binaries
//   rir::deserializerData::iterateOverUnits(ddContainer, [&](SEXP ddContainer, SEXP offsetUnitContainer, SEXP contextUnitContainer, SEXP binaryUnitContainer) {
//     std::stringstream binInPathPrefix, binOutPathPrefix;

//     binInPathPrefix << inputPath << "/" << CHAR(PRINTNAME(rir::deserializerData::getHast(ddContainer))) 
//                            << "_" << rir::offsetUnit::getOffsetIdxAsInt(offsetUnitContainer)
//                            << "_" << CHAR(PRINTNAME(rir::binaryUnit::getEpoch(binaryUnitContainer)));

//     binOutPathPrefix << outputPath  << "/" << CHAR(PRINTNAME(rir::deserializerData::getHast(ddContainer))) 
//                            << "_" << rir::offsetUnit::getOffsetIdxAsInt(offsetUnitContainer)
//                            << "_" << CHAR(PRINTNAME(rir::binaryUnit::getEpoch(binaryUnitContainer)));

//     {
//       // Copy BC
//       std::ifstream  src(binInPathPrefix.str() + ".bc", std::ios::binary);
//       std::ofstream  dst(binOutPathPrefix.str() + ".bc",   std::ios::binary);

//       dst << src.rdbuf();
//     }

//     {
//       // Copy BC
//       std::ifstream  src(binInPathPrefix.str() + ".pool", std::ios::binary);
//       std::ofstream  dst(binOutPathPrefix.str() + ".pool",   std::ios::binary);
      
//       dst << src.rdbuf();
//     }

//   });
// }

// static void testSavedDMeta(const std::string & metaFilename) {
//   std::stringstream path;
//   path << outputPath << "/" << metaFilename << "d";

//   FILE *reader;
//   reader = fopen(path.str().c_str(),"r");

//   if (!reader) {
//     for (int i = 0; i < 10; i++) {
//       sleep(1);
//       reader = fopen(path.str().c_str(),"r");
//       if (reader) break;
//     }

//     if (!reader) {
//       std::cerr << "unable to open " << path.str() << std::endl;
//       Rf_error("unable to open file!");
//     }
//   }

//   // Initialize the deserializing stream
//   R_inpstream_st inputStream;
//   R_InitFileInPStream(&inputStream, reader, R_pstream_binary_format, NULL, R_NilValue);

//   rir::Protect protecc;
//   SEXP ddContainer;
//   protecc(ddContainer= R_LoadFromFile(reader, 0));
//   fclose(reader);

//   // rir::deserializerData::print(ddContainer, 2);
// }

class SCElement {
public:
  SCElement(SEXP container, int index) : _container(container), _index(index) {
    auto tag = rir::speculativeContextElement::getTag(container);
    std::stringstream ss;
    ss << tag << "_" << rir::speculativeContextElement::getPOD(container) << "_";
    if (tag == 0 || tag == 1) {
      ss << rir::speculativeContextElement::getValUint(container);
    } else {
      SEXP val = rir::speculativeContextElement::getValSEXP(container);
      if (TYPEOF(val) == INTSXP) {
        if (INTEGER(val)[0] == -1) {
          ss << "NonHast";
        } else if (INTEGER(val)[0] == -2) {
          ss << "NonRirClos";
        } else if (INTEGER(val)[0] == -3) {
          ss << "NonMono";
        } else {
          ss << "Special(" << INTEGER(val)[0] << ")";
        }
      } else {
        assert(TYPEOF(val) == SYMSXP);
        ss << CHAR(PRINTNAME(val));
      }
    }
    _name = ss.str();
    if (conversionMap.count(_name) == 0) {
      conversionMap[_name] = idx++;
    }
  }

  size_t getConvertedId() const {
    return conversionMap[_name];
  }

  std::string getName() {
    return _name;
  }

  bool isDeopt() const {
    return rir::speculativeContextElement::getPOD(_container);
  }

  void print(std::ostream& out) {
    rir::speculativeContextElement::print(_container);
  }

private:
  static size_t idx;
  static std::unordered_map<std::string, size_t> conversionMap;
  SEXP _container;
  std::string _name;
  int _index;
};

size_t SCElement::idx = 0;
std::unordered_map<std::string, size_t> SCElement::conversionMap;

class FeedbackVector {
public:
  FeedbackVector() {}
  FeedbackVector(SEXP scVector) {
    for (int j = 0; j < Rf_length(scVector); j++) {
      SEXP ele = VECTOR_ELT(scVector, j);
      _container.push_back(SCElement(ele, j));
    }

    std::stringstream ss;
    ss << "|";
    for (auto & ele : _container) {
      ss << ele.getConvertedId() << "|";
    }
    _uid = ss.str();
  }

  void print(std::ostream& out, const int & space) const {
    printSpace(space, out);
    out << "╠═(vector=" << _container.size() << "): [ ";
    for (auto & ele : _container) {
      if (ele.isDeopt()) {
        out << "[" << ele.getConvertedId() << "] ";
      } else {
        out << ele.getConvertedId() << " ";
      }
    }
    out << "]" << std::endl;
  }

  size_t numDeoptPoints() const {
    size_t res = 0;
    for (auto & ele : _container) {
      res += ele.isDeopt() ? 1 : 0;
    }
    return res;
  }

  SCElement get(const size_t & idx) {
    return _container[idx];
  }

  static std::set<size_t> getDiff(const FeedbackVector & fv1, const FeedbackVector & fv2) {
    std::set<size_t> res;
    assert(fv1._container.size() == fv2._container.size());
    for (size_t i = 0; i < fv1._container.size(); i++) {
      if (fv1._container[i].getConvertedId() != fv2._container[i].getConvertedId()) res.insert(i);
    }
    return res;
  }

  std::string getUID() const {
    return _uid;
  }

  bool operator==(const FeedbackVector& other) const {
    return _uid == other._uid;
  }

private:
  std::string _uid;
  std::vector<SCElement> _container;
};

class SpeculativeContext {
public:
  SpeculativeContext(SEXP scContainer) {
    for (int i = 0; i < Rf_length(scContainer); i++) {
      SEXP curr = VECTOR_ELT(scContainer, i);
      SEXP currHast = VECTOR_ELT(curr, 0);
      SEXP currCon = VECTOR_ELT(curr, 1);
      FeedbackVector fv(currCon);
      _container.emplace(currHast,fv);
    }
  }

  bool containsVector(SEXP hast) {
    return _container.count(hast) > 0;
  }

  FeedbackVector getVector(SEXP hast) {
    assert(containsVector(hast));
    return _container[hast];
  }

  size_t numDeoptPoints() const {
    size_t res = 0;
    for (auto & ele : _container) {
      res += ele.second.numDeoptPoints();
    }
    return res;
  }

  bool equals(SpeculativeContext& other) {
    if (_container.size() != other._container.size()) return false;
    for (auto & ele : _container) {
      if (other._container.count(ele.first) == 0) return false;
    }
    for (auto & ele : _container) {
      auto currFV = ele.second;
      auto otherFV = other._container[ele.first];
      if (!(currFV == otherFV)) return false;
    }
    return true;
  }

  std::set<SEXP> getHasts() {
    std::set<SEXP> res;
    for (auto & ele : _container) {
      res.insert(ele.first);
    }
    return res;
  }

  void print(std::ostream& out, const int & space) const {
    printSpace(space, out);
    out << "╠═(speculativeContext=" << _container.size() << ",deoptPoints=" << numDeoptPoints() << ")" << std::endl;
    for (auto & ele : _container) {
      printSpace(space+2, out);
      out << "╠═(hast=" << CHAR(PRINTNAME(ele.first)) << ")" << std::endl;
      ele.second.print(out, space+4);
    }
  }
private:
  std::unordered_map<SEXP, FeedbackVector> _container;
};

class SBinary {
public:
  SBinary(rir::Context context, SEXP epoch, SEXP cData) 
    : _context(context), _epoch(epoch), _speculativeContext(rir::contextData::getSpeculativeContext(cData)) {
    auto rData = rir::contextData::getReqMapAsVector(cData);
    for (int i = 0; i < Rf_length(rData); i++) {
      SEXP ele = VECTOR_ELT(rData, i);
      assert(TYPEOF(ele) == SYMSXP);
      _dependencies.push_back(ele);
    }
  }

  SpeculativeContext getSpeculativeContext() {
    return _speculativeContext;
  }

  void print(std::ostream& out, const int & space) const {
    printSpace(space, out);
    out << "█═(SBinary)" << std::endl;
    printSpace(space, out);
    out << "╠═(epoch=" << CHAR(PRINTNAME(_epoch)) << ")" << std::endl;
    printSpace(space, out);
    out << "╠═(dependencies=" << _dependencies.size() << "): [ ";
    for (auto & dep : _dependencies) {
      out << CHAR(PRINTNAME(dep)) << " ";
    }
    out << "]" << std::endl;

    _speculativeContext.print(out, space+2);
  }

  std::string getEpoch() const {
    return std::string(CHAR(PRINTNAME(_epoch)));
  }

  bool operator==(const SBinary& other) const {
    return getEpoch() == other.getEpoch();
  }

private:
  rir::Context _context;
  SEXP _epoch;
  std::vector<SEXP> _dependencies;
  SpeculativeContext _speculativeContext;
};

class SBinaryHash {
public:
  size_t operator()(const SBinary& sb) const
  {
    return (std::hash<std::string>()(sb.getEpoch()));
  }
};

class SpecializedBinaries {
public:
  SpecializedBinaries(SEXP container, SEXP offset, SEXP hast, SEXP name) 
    : _offset(offset), _hast(hast), _name(name) {
    assert(TYPEOF(container) == ENVSXP);
    REnvHandler handler(container);
    handler.iterate([&] (SEXP epochSym, SEXP cData) {
      rir::Context con(rir::contextData::getContext(cData));
      _binaries[con].push_back(SBinary(con, epochSym, cData));
    });
  }

  std::vector<SBinary> getBinaries(const rir::Context & con) {
    return _binaries[con];
  }

  std::vector<rir::Context> getContexts() {
    std::vector<rir::Context> res;
    for (auto & ele : _binaries) {
      res.push_back(ele.first);
    }
    return res;
  }

  void reduceRedundantBinaries() {
    std::unordered_map<rir::Context, std::vector<SBinary>> reduced;
    for (auto & ele : _binaries) {
      auto context = ele.first;
      auto binaries = ele.second;
      std::set<size_t> toSkip;
      for (size_t i = 0; i < binaries.size(); i++) {
        if (toSkip.count(i) > 0) continue;
        auto binA = binaries[i];
        for (size_t j = 0; j < binaries.size(); j++) {
          if (toSkip.count(j) > 0) continue;
          auto binB = binaries[j];
          auto binBCon = binB.getSpeculativeContext();
          if (binA.getSpeculativeContext().equals(binBCon)) {
            toSkip.insert(j);
          }
        }
        reduced[context].push_back(binA);
      }
    }
    _binaries = reduced;
  }

  void print(std::ostream& out, const int & space) {
    printSpace(space, out);
    out << "█═════════(offset=" << CHAR(PRINTNAME(_offset)) << ",hast=" << CHAR(PRINTNAME(_hast)) << ")═════════█" << std::endl;
    int i = 1;
    for (auto & ele : _binaries) {
      printSpace(space + 2, out);
      out << "╠═(context=" << ele.first << ")" << std::endl;
      for (auto & bin : ele.second) {
        printSpace(space + 4, out);
        out << "█═(binary=" << i++ << ")" << std::endl;
        bin.print(out, space + 6);
      }
    }
  }
private:
  SEXP _offset, _hast, _name;
  std::unordered_map<rir::Context, std::vector<SBinary>> _binaries;
};


class FeedbackVectorHash {
public:
  size_t operator()(const FeedbackVector& fv) const
  {
    return (std::hash<std::string>()(fv.getUID()));
  }
};

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

class SSolution {
public:
  void addSolution(SEXP criteria, std::set<size_t> soln) {
    for (auto & s : soln) {
      _solution[criteria].insert(s);
    }
  }
  std::unordered_map<SEXP, std::set<size_t>> getSolution() {
    return _solution;
  }
  void print(std::ostream& out, const int & space) const {
    printSpace(space, out);
    out << "█═(ssol=" << _solution.size() << ")" << std::endl;
    for (auto & ele : _solution) {
      printSpace(space + 2, out);
      out << "=(criteria=" << CHAR(PRINTNAME(ele.first)) << "): [ ";
      for (auto & s : ele.second) {
        out << s << " ";
      }
      out << "]" << std::endl;
    }
  }
private:
  std::unordered_map<SEXP, std::set<size_t>> _solution;
};

class SlotSelectionSolution  {
public:
  
  void addSolution(SEXP criteria, std::vector<SBinary> bins, std::set<size_t> solIdx) {
    for (auto & binary : bins) {
      _solution[binary].addSolution(criteria, solIdx);
    }
  }

  void print(std::ostream& out, const int & space) const {
    printSpace(space, out);
    out << "=(slotselectionsol=" << _solution.size() << ")" << std::endl;
    for (auto & ele : _solution) {
      ele.first.print(out, space + 2);
      ele.second.print(out, space + 2);
    }
  }
private:
  std::unordered_map<SBinary, SSolution, SBinaryHash> _solution;
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
        printSpace(2);
        std::cout << "solving recursively" << std::endl;
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
        printSpace(2);
        std::cout << "Working with recurse map" << std::endl;
        for (auto & ele : recurseMap) {
          if (used.count(ele.first) > 0) continue;
          if (ele.second.size() > 1) {
            SlotSelectionGraph ssg(ele.first);
            ssg.addBinaries(ele.second);
            ssg.init();
            ssg.print(std::cout, 4);
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

size_t SlotSelectionGraph::BUDGET = 10;
std::set<SEXP> SlotSelectionGraph::used;



static void iterateOverMetadatasInDirectory() {
  DIR *dir;
  struct dirent *ent;

  // size_t functionsSeen = 0;
  // size_t functionsWithMoreThanOneContext = 0, functionsMasked = 0;
  
  if ((dir = opendir(inputPath.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      std::string fName = ent->d_name;
      if (fName.find(".meta") != std::string::npos) {

        // if (obapDEBUG) {
        //   std::cerr << "OBAP processing: " << fName << std::endl;
        // }

        std::stringstream metadataPath;
        metadataPath << inputPath << "/" << fName;

        FILE *reader;
        reader = fopen(metadataPath.str().c_str(),"r");

        if (!reader) {
          for (int i = 0; i < 10; i++) {
            sleep(1);
            reader = fopen(metadataPath.str().c_str(),"r");
            if (reader) break;
          }

          if (!reader) {
            std::cerr << "unable to open " << metadataPath.str() << std::endl;
            Rf_error("unable to open file!");
            continue;
          }
        }


        SEXP serDataContainer;
        rir::Protect protecc;
        protecc(serDataContainer= R_LoadFromFile(reader, 0));
        fclose(reader);

        // printSpace(0);
        // std::cout << "Processing: " << fName << std::endl;
        // printSpace(2);
        // std::cout << "FunctionName: " << CHAR(PRINTNAME(rir::serializerData::getName(serDataContainer))) << std::endl;

        // rir::serializerData::print(serDataContainer, 0);

        // Get serialized metadata
        REnvHandler offsetMapHandler(rir::serializerData::getBitcodeMap(serDataContainer));
        SEXP hast = rir::serializerData::getHast(serDataContainer);
        offsetMapHandler.iterate([&] (SEXP offsetIndex, SEXP contextMap) {
          SpecializedBinaries sBins(contextMap, offsetIndex, hast, rir::serializerData::getName(serDataContainer));
          sBins.reduceRedundantBinaries();
          sBins.print(std::cout, 0);

          for (auto & context : sBins.getContexts()) {
            printSpace(2);
            std::cout << "█[SOLUTION]█══(context=" << context << ")" << std::endl;
            SlotSelectionGraph ssg(hast); // starting criteria is always the main 'Hast'
            ssg.addBinaries(sBins.getBinaries(context));
            ssg.init();
            // ssg.print(std::cout, 2);
            SlotSelectionSolution solnHolder;
            ssg.findSolution(&solnHolder);
            solnHolder.print(std::cout, 4);
            // std::cout << "====================================================" << std::endl;
          }

          // std::stringstream pathPrefix;
          // pathPrefix << inputPath << "/" << CHAR(PRINTNAME(rir::serializerData::getHast(serDataContainer))) << "_" << CHAR(PRINTNAME(offsetIndex)) << "_";

        });


        // if (obapDEBUG) {
        //   rir::deserializerData::print(ddContainer, 2);
        // }
        
        // saveDMetaAndCopyFiles(ddContainer, fName);
        // testSavedDMeta(fName);

      }
    }

    // SerializedDataProcessor::printStats(0);

    // printSpace(0);
    // std::cout << "Collisions                : " << TVGraph::collisions << std::endl;

    // printSpace(0);
    // std::cout << "Functions Unique          : " << functionsSeen << std::endl;
    
    // printSpace(0);
    // std::cout << "Functions (>1) Contexts   : " << functionsWithMoreThanOneContext << std::endl;
    // printSpace(0);
    // std::cout << "Functions Masked          : " << functionsMasked << std::endl;
  
  } else {
    std::cerr << "\"" << inputPath << "\" has no metas, nothing to process" << std::endl;
    exit(EXIT_FAILURE);
  }
}

// https://stackoverflow.com/questions/46728680/how-to-temporarily-suppress-output-from-printf
int supress_stdout() {
  fflush(stdout);

  int ret = dup(1);
  int nullfd = open("/dev/null", O_WRONLY);
  // check nullfd for error omitted
  dup2(nullfd, 1);
  close(nullfd);

  return ret;
}

void resume_stdout(int fd) {
  fflush(stdout);
  dup2(fd, 1);
  close(fd);
}


int main(int argc, char** argv) {
  bool rHomeSet = getenv("R_HOME") ? true : false;

  if (!rHomeSet) {
    std::cerr << "R_HOME not set, set this to the GNUR built using the ./configure --enable-R-shlib flag" << std::endl;
    std::cerr << "Usage: ./run.sh [path_to_folder] [output_folder]" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (argc != 4) {
    std::cerr << "Usage: ./run.sh [path_to_folder] [output_folder]" << std::endl;
    exit(EXIT_FAILURE);
  }

  inputPath = argv[2];
  outputPath = argv[3];

  std::cout << "[OBAP Started]" << std::endl;
  std::cout << "Raw Bitcodes Folder       : " << inputPath << std::endl;
  std::cout << "Processed Bitcodes Folder : " << outputPath << std::endl;

  std::cout.setstate(std::ios_base::failbit);
  int fd = supress_stdout();
  int status = Rf_initEmbeddedR(argc, argv);
  if (status < 0) {
    std::cerr << "R initialization failed." << std::endl;
    exit(EXIT_FAILURE);
  }
  resume_stdout(fd);
  std::cout.clear();

  RshBuiltinWeights::init();

  iterateOverMetadatasInDirectory();

  Rf_endEmbeddedR(0);
  return 0;
}
