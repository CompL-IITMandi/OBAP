#pragma once

#include "opt/ModuleManager.h"

class OBAHolder {
  public:
    OBAHolder() = delete;
    OBAHolder(const std::string & pathPrefix) : _pathPrefix(pathPrefix) {
      init();
    }
    void print(const unsigned int & space);

  private:
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
