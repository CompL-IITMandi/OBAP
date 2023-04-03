#pragma once
#include "Rinternals.h"
#include <string>
#include <unordered_map>
#include <sstream>
#include <vector>
#include "serializerData.h"
#include "utils/Debug.h"

class SCElement {
public:
  SCElement(SEXP container, int index) : _container(container), _index(index) {
    auto tag = rir::speculativeContextElement::getTag(container);
    std::stringstream ss;
    ss << tag << "_";
    if (tag == 0 || tag == 1 || tag == 3 || tag == 4) {
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
      // std::cout << "Adding SCElement: " << _name << std::endl;
      conversionMap[_name] = idx++;
    }
  }

  uint32_t getValUint() {
    return rir::speculativeContextElement::getValUint(_container);
  }

  int getTag() {
    return rir::speculativeContextElement::getTag(_container);
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

  SEXP getContainer() {
    return _container;
  }

private:
  static size_t idx;
  static std::unordered_map<std::string, size_t> conversionMap;
  SEXP _container;
  std::string _name;
  int _index;
};

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
    int i = 0;
    for (auto & ele : _container) {
      if (ele.isDeopt()) {
        out << "(" << i++ << ")" << "[" << ele.getConvertedId() << "] ";
      } else {
        out << "(" << i++ << ")" << ele.getConvertedId() << " ";
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

  std::vector<SCElement> getContainer() {
    return _container;
  }

  size_t size() {
    return _container.size();
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

  std::unordered_map<SEXP, FeedbackVector> getContainer() {
    return _container;
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

  rir::Context getContext() {
    return _context;
  }

  std::string getEpoch() const {
    return std::string(CHAR(PRINTNAME(_epoch)));
  }

  SEXP getEpochAsSEXP() const {
    return _epoch;
  }

  std::vector<SEXP> getDependencies() const {
    return _dependencies;
  }

  SpeculativeContext getSpeculativeContext() const {
    return _speculativeContext;
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

class SlotSelectionSolution {
public:
  
  void addSolution(SEXP criteria, std::vector<SBinary> bins, std::set<size_t> solIdx) {
    for (auto & binary : bins) {
      _solution[binary].addSolution(criteria, solIdx);
    }
  }

  std::unordered_map<SBinary, SSolution, SBinaryHash> get() { return _solution; }

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

  void addSolution(rir::Context context, std::unordered_map<SBinary, SSolution, SBinaryHash> sol) {
    _solution[context] = sol;
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

  std::unordered_map<rir::Context, std::unordered_map<SBinary, SSolution, SBinaryHash>> getSolution() {
    return _solution;
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
      auto sols = _solution[ele.first];
      for (auto & bin : ele.second) {
        printSpace(space + 4, out);
        out << "█═(binary=" << i++ << ")" << std::endl;
        if (sols.count(bin) > 0) {
          printSpace(space + 6, out);
          out << "█═(Contains Solution)" << std::endl;
          sols[bin].print(out,space + 8);
        } else {
          printSpace(space + 6, out);
          out << "█═(No Solution)" << std::endl;
        }
        bin.print(out, space + 8);
        
      }
    }
  }
private:
  SEXP _offset, _hast, _name;
  std::unordered_map<rir::Context, std::vector<SBinary>> _binaries;
  std::unordered_map<rir::Context, std::unordered_map<SBinary, SSolution, SBinaryHash>> _solution;
};


class FeedbackVectorHash {
public:
  size_t operator()(const FeedbackVector& fv) const {
    return (std::hash<std::string>()(fv.getUID()));
  }
};