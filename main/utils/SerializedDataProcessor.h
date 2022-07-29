#pragma once

#include "Rinternals.h"
#include <unordered_map>
#include <vector>
#include <string>

#include "utils/Debug.h"

// This class is responsible for processing a contextData
class SerializedDataProcessor {
  public:
    SerializedDataProcessor(SEXP cData, const std::string & pathPrefix) : _cData(cData), _pathPrefix(pathPrefix) {}

    // Iterate over all contexts
    //  1. For each context context perform TV reduction
    //  2. Perform slot selection over reduced TVs
    //  2. Compare and curb contexts
    //  3. Generate context mask
    void init();

    // Prints the final results of processing
    void print(const unsigned int & space);

  private:
    SEXP _cData;
    std::string _pathPrefix;
    unsigned int _origBitcodes = 0;
    unsigned int _deprecatedContexts = 0;
    unsigned int _deprecatedBitcodes = 0;
    std::unordered_map<unsigned long, std::vector<SEXP>> _origContextWiseData;

};

// class GlobalData {
//   public:
//     static std::string bitcodesFolder;
// };

// // AnalysisCallback
// // 1. (vector) contexts
// // 2. (unordered map) weight analysis
// // 3. (unordered map) simple argument effect analysis, for each argument get a vector of called functions
// // 4. (unordered map) breadth first call data, get leveled function call data
// typedef std::function<void(
//                           std::vector<rir::Context> &, 
//                           std::unordered_map<rir::Context, unsigned> &, 
//                           std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > &,
//                           std::unordered_map<rir::Context, std::vector<std::set<std::string>>> &
//                           )>
//                             AnalysisCallback;

// // void doAnalysisOverContexts(const std::string & pathPrefix, json & contextMap, AnalysisCallback call);
// void doAnalysisOverContexts(const std::string & pathPrefix, SEXP cDataContainer, AnalysisCallback call);


// enum ComparisonType {
//   STRICT,      // Strictly comparable
//   ROUGH_EQ,    // Rough comparable - Equal
//   ROUGH_NEQ,   // Rough comparable - Lattice comparable
// };

// // ComparisonCallback
// // 1. (Context) context1
// // 2. (Context) context2
// // 3. (ComparisonType) type
// typedef std::function<void(
//                           rir::Context &,
//                           rir::Context &,
//                           const ComparisonType &
//                           )>
//                             ComparisonCallback;
// void compareContexts(std::vector<rir::Context> & contextsVec, ComparisonCallback call);
