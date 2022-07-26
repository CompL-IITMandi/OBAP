#pragma once
#include "runtime/Context.h"
#include <iostream>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>
#include <set>
typedef struct SEXPREC* SEXP;

class GlobalData {
  public:
    static std::string bitcodesFolder;
};

// AnalysisCallback
// 1. (vector) contexts
// 2. (unordered map) weight analysis
// 3. (unordered map) simple argument effect analysis, for each argument get a vector of called functions
// 4. (unordered map) breadth first call data, get leveled function call data
typedef std::function<void(
                          std::vector<rir::Context> &, 
                          std::unordered_map<rir::Context, unsigned> &, 
                          std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > &,
                          std::unordered_map<rir::Context, std::vector<std::set<std::string>>> &
                          )>
                            AnalysisCallback;

// void doAnalysisOverContexts(const std::string & pathPrefix, json & contextMap, AnalysisCallback call);
void doAnalysisOverContexts(const std::string & pathPrefix, SEXP cDataContainer, AnalysisCallback call);


enum ComparisonType {
  STRICT,      // Strictly comparable
  ROUGH_EQ,    // Rough comparable - Equal
  ROUGH_NEQ,   // Rough comparable - Lattice comparable
};

// ComparisonCallback
// 1. (Context) context1
// 2. (Context) context2
// 3. (ComparisonType) type
typedef std::function<void(
                          rir::Context &,
                          rir::Context &,
                          const ComparisonType &
                          )>
                            ComparisonCallback;
void compareContexts(std::vector<rir::Context> & contextsVec, ComparisonCallback call);
