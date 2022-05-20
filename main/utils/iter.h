#ifndef JSON_ITER_H
#define JSON_ITER_H

#include "nlohmann/json.hpp"
#include "rir/Context.h"
#include <iostream>
#include <functional>
#include <set>

typedef struct SEXPREC* SEXP;

using json = nlohmann::json;

class GlobalData {
  public:
    static std::string bitcodesFolder;
};

// IterCallback
// 1. (string) meta file name
// 2. (string) hast
// 3. (string) name
// 4. (string) offset
// 4. (json) contextMap

typedef std::function<void(
                          const std::string &, 
                          const std::string &, 
                          const std::string &, 
                          const std::string &, 
                          json &)>
                            IterCallback;

// AnalysisCallback
// 1. (vector) contexts
// 2. (unordered map) weight analysis
// 3. (unordered map) simple argument effect analysis, for each argument get a vector of called functions
// 4. (unordered map) breadth first call data, get leveled function call data
typedef std::function<void(
                          std::vector<Context> &, 
                          std::unordered_map<Context, unsigned> &, 
                          std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > &,
                          std::unordered_map<Context, std::vector<std::set<std::string>>> &
                          )>
                            AnalysisCallback;

void iterateOverBitcodes(json & processedJson, IterCallback call);

// void doAnalysisOverContexts(const std::string & pathPrefix, json & contextMap, AnalysisCallback call);
void doAnalysisOverContexts(const std::string & pathPrefix, SEXP cDataContainer, AnalysisCallback call);


enum ComparisonType {
  STRICT,      // Strictly comparable
  ROUGH,       // Roughly comparable, missingness is different.
  DIFFZEROMISS,// Not comparable, missingness is zero for both
  DIFFSAMEMISS,// Not comparable, missingness is same
  DIFFDIFFMISS // Not comparable, missingness is different
};

// ComparisonCallback
// 1. (Context) context1
// 2. (Context) context2
// 3. (ComparisonType) type
typedef std::function<void(
                          Context &,
                          Context &,
                          const ComparisonType &
                          )>
                            ComparisonCallback;
void compareContexts(std::vector<Context> & contextsVec, ComparisonCallback call);

#endif