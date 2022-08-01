#pragma once

#include "opt/ModuleManager.h"

struct ComparisonResult {
  ComparisonResult(bool similar, unsigned argEffectResult, std::vector<unsigned> similarArgs) : similar(similar), argEffectResult(argEffectResult), similarArgs(similarArgs) {}
  bool similar;
  // 0 -> [Non conclusive] There were no comparable indices
  // 1 -> [All comparable were similar] The number of comparable indices were > 0 but the arglist size was different
  // 2 -> [All comparable were similar] The number of comparable indices were > 0 and arglist were identical
  // 3 -> [Not similar] If any comparable arg was different, then this is returned
  // 4 -> [Not similar] If unexpected error occurs
  unsigned argEffectResult;
  std::vector<unsigned> similarArgs;
};

class OBAHolder {
  // Threshold for similarity, 0/strictest by default
  static unsigned THRESHOLD_WEIGHT;
  static unsigned THRESHOLD_FUNCALL;
  static unsigned THRESHOLD_ARGEFFECT;

  public:
    OBAHolder() = delete;
    OBAHolder(const std::string & pathPrefix) : _pathPrefix(pathPrefix) {
      init();
    }
    void print(const unsigned int & space) const;

    ComparisonResult equals(const OBAHolder& other);

  private:
    // 
    // Local vars
    // 
    const std::string _pathPrefix;
    std::string _mainFunHandle;
    // 
    // Analysis Results
    // For funCallBF and weightAnalysis we only keep the main functions result as matching promises is not always possible.
    // 
    unsigned _analysisWeight = 0;
    std::vector<std::pair<unsigned, std::vector<std::string>>> _analysisArgEffect;
    std::vector<std::set<std::string>> _funCallBF;

    void init();

    // 1. Weight Analysis Result
    void storeWeightAnalysis(ModuleManager & MM);

    // 2. ArgEffect Analysis Result
    void storeArgEffectAnalysis(ModuleManager & MM);

    // 3. FunCallBF Analysis Result
    void storeFunCallBFAnalysis(ModuleManager & MM);
};
