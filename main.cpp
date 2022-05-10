#include <iostream>
#include <stdlib.h>
#include <functional>
#include <fstream>
#include <algorithm>
#include <set>
#include <iterator>

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"

#include "rir/Context.h"
#include "nlohmann/json.hpp"
#include "opt/ModuleManager.h"
#include "utils/hasse.h"

#include "utils/RshBuiltinsMap.h"
#include "utils/iter.h"
#include "utils/ContextAnalysisComparison.h"

using json = nlohmann::json;

#define PRINT_PROGRESS 0

// 0 - callOrderSimilarity || directly comparable && counter && argument
// 1 - directly comparable && counter
// 2 - roughly comparable && counter
// 3 - counter 
// 4 - counter || argument
unsigned MASKING_RISK = 4;

std::unordered_map<std::string, unsigned> RshBuiltinWeights::weightMap;

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: bcp path_to_folder" << std::endl;
    exit(EXIT_FAILURE);
  }

  auto bitcodesFolder = argv[1];
  GlobalData::bitcodesFolder = bitcodesFolder;

  std::stringstream jsonFileLocation;
  jsonFileLocation << bitcodesFolder << "/summary.json"; 

  std::ifstream stream(jsonFileLocation.str().c_str());
  if (!stream) {
    std::cerr << "Unable to open json at: " << jsonFileLocation.str() << std::endl;
    exit(EXIT_FAILURE);
  }

  std::stringstream maskDataLocation;
  maskDataLocation << bitcodesFolder << "/maskData"; 
  std::ofstream maskDataStream(maskDataLocation.str().c_str());
  if (!stream) {
    std::cerr << "Unable to create file maskData" << std::endl;
    exit(EXIT_FAILURE);
  }

  RshBuiltinWeights::init();

  

  // Load the processed JSON data
  // We converted the .meta files to JSON to make it easier to work with and 
  // end dependence on GNUR for doing other things.
  json processedJson;
  stream >> processedJson;

  iterateOverBitcodes(
    processedJson,
    [&] (
          const std::string & meta, 
          const std::string & hast, 
          const std::string & name,
          const std::string & offset, 
          json & contextMap
          ) {
      auto contextsAvailable = contextMap.size();
      if (contextsAvailable == 1) return; // Nothing to process when we cant compare
      std::cout << "File: " << meta << std::endl;
      std::cout << "  Hast: " << hast << std::endl;
      std::cout << "  Name: " << name << std::endl;
      std::cout << "  Offset: " << offset << std::endl;
      std::cout << "  Contexts Available: " << contextsAvailable << std::endl;

      std::stringstream pathPrefix;
      pathPrefix << bitcodesFolder << "/" << hast << "_" << offset << "_";
      std::cout << "  Prefix Path: " << pathPrefix.str() << std::endl;

      doAnalysisOverContexts(
        pathPrefix.str(),
        contextMap,
        [&] (
          std::vector<Context> & contextsVec, 
          std::unordered_map<Context, unsigned> & weightAnalysis, 
          std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis,
          std::unordered_map<Context, std::vector<std::set<std::string>>> & funCallBFData
        )  {
          compareContexts(contextsVec, [&] (Context & c1, Context & c2, const ComparisonType & t) {
            if (t == ComparisonType::STRICT) {
              std::cout << "    [STRICT]: " << c1 << " || " << c2 << std::endl;
            } else if (t == ComparisonType::ROUGH) {
              std::cout << "    [ROUGH]: " << c1 << " || " << c2 << std::endl;
            } else if (t == ComparisonType::DIFFZEROMISS) {
              std::cout << "    [DIFFZEROMISS]: " << c1 << " || " << c2 << std::endl;
            } else if (t == ComparisonType::DIFFSAMEMISS) {
              std::cout << "    [DIFFSAMEMISS]: " << c1 << " || " << c2 << std::endl;
            } else if (t == ComparisonType::DIFFDIFFMISS) {
              std::cout << "    [DIFFDIFFMISS]: " << c1 << " || " << c2 << std::endl;
            }

            ContextAnalysisComparison cac(c1, c2, t);
            std::cout << "      [MASK]: " << cac.getMask(weightAnalysis, simpleArgumentAnalysis, funCallBFData) << std::endl;
          });
        }
      );
    }
  );
  maskDataStream.close();
  return 0;
}
