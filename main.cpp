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

  std::stringstream jsonFileLocation;
  jsonFileLocation << bitcodesFolder << "/summary.json"; 

  unsigned counterThreshold = 5;

  unsigned argumentEffectDifference = 10;


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

  for (json::iterator it_metas = processedJson.begin(); it_metas != processedJson.end(); ++it_metas) {
    auto currHastJson = it_metas.value();

    #if PRINT_PROGRESS == 1
    std::cout << "  -- " << it_metas.key() << std::endl;
    std::cout << "     hast: " << currHastJson["hast"].get<std::string>() << std::endl;
    #endif

    auto offsetsAvailable = currHastJson["offsetMap"];
    #if PRINT_PROGRESS == 1
    std::cout << "     offsets:" << std::endl;
    #endif

    for (json::iterator currOffset = offsetsAvailable.begin(); currOffset != offsetsAvailable.end(); ++currOffset) {
      #if PRINT_PROGRESS == 1
      std::cout << "     -- " << currOffset.key() << std::endl;
      #endif

      std::vector<Context> contextsVec;

      auto contextsAvailable = currOffset.value();
      if (contextsAvailable.size() == 1) {
        continue;
      }

      std::unordered_map<Context, unsigned> weightAnalysis;
      std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > simpleArgumentAnalysis;
      std::unordered_map<Context, std::vector<std::set<std::string>>> funCallBFData;

      for (json::iterator currContext = contextsAvailable.begin(); currContext != contextsAvailable.end(); ++currContext) {
        std::string conStr = currContext.value()["context"];
        unsigned long con = std::stoul(conStr);
        Context c(con);

        #if PRINT_PROGRESS == 1
        std::cout << "       -- (" << con << ") " << c << std::endl;
        #endif

        contextsVec.push_back(c);          
        
        std::stringstream bitcodePath;
        bitcodePath << bitcodesFolder << "/" << currHastJson["hast"].get<std::string>() << "_" << currOffset.key() << "_" << conStr << ".bc";

        // load bitcode
        llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> mb = llvm::MemoryBuffer::getFile(bitcodePath.str().c_str());

        llvm::orc::ThreadSafeContext TSC = std::make_unique<llvm::LLVMContext>();

        // Load the memory buffer into a LLVM module
        llvm::Expected<std::unique_ptr<llvm::Module>> llModuleHolder = llvm::parseBitcodeFile(mb->get()->getMemBufferRef(), *TSC.getContext());

        // ModuleManager to run our passes on the bitcodes
        ModuleManager MM(*llModuleHolder.get().get());

        MM.runPasses();

        auto callSiteCounterRes = MM.getRshCallSiteCounterRes();

        unsigned weight = 0;

        for (auto & ele : callSiteCounterRes) {
          weight += RshBuiltinWeights::getWeight(ele.first().str());
        }
        weightAnalysis[c] = weight;

        std::string mainFunName = currContext.value()["function_names"][0];

        auto argTrackRes = MM.getRshArgumentEffectSimpleRes();
        for (auto & ele : argTrackRes) {
          auto currFun = ele.first;
          auto currFunData = ele.second;
          if (currFun->getName().str() == mainFunName) {
            simpleArgumentAnalysis[c] = currFunData;
            break;
          }
        }

        auto funcCallBFRes = MM.getFunctionCallBreathFirstRes();
        for (auto & ele : funcCallBFRes) {
          auto currFunName = ele.first().str();
          auto currFunData = ele.second;
          if (currFunName == mainFunName) {
            std::vector<std::set<std::string>> fc;
            for (auto & e : currFunData) fc.push_back(e.getFunctionSet());
            funCallBFData[c] = fc;
            break;
          }
        }
        
      }

      std::cout << "HAST: " << currHastJson["hast"].get<std::string>() << std::endl;

      Context mask;
      for (auto & currCon : contextsVec) {
        // std::cout << "working with context: " << currCon << std::endl;
        std::unordered_map<unsigned, std::vector<std::string>> origArgMap;

        for (auto & data : simpleArgumentAnalysis[currCon]) {
          unsigned argIdx = data.first;
          auto calledFuns = data.second;
          origArgMap[argIdx] = calledFuns;
        }

        auto currEffectedArgs = currCon.getAffectedArguments();

        for (auto & other : contextsVec) {
          if (currCon == other) continue;

          auto uselessCon = other - currCon;

          bool counterSimilarity = (weightAnalysis[currCon] - weightAnalysis[other] <= counterThreshold) || (weightAnalysis[other] - weightAnalysis[currCon] <= counterThreshold);
          
          auto currV = funCallBFData[currCon];
          auto otherV = funCallBFData[other];
          auto levelsInCurrent = currV.size();
          auto levelsInOther = otherV.size();
          unsigned callOrderDifference = 0;

          if (levelsInCurrent == levelsInOther) {
            for (unsigned i = 0; i < levelsInCurrent; i++) {
              auto v1 = currV[i];
              auto v2 = otherV[i];
              std::vector<std::string> diff;
              //no need to sort since it's already sorted
              std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
                std::inserter(diff, diff.begin()));
              if (diff.size() > 0) {
                callOrderDifference++;
              }
            }
          }

          if (levelsInCurrent > 0 && levelsInCurrent == levelsInOther) {
            if (callOrderDifference == 0) {
              std::cout << "callOrderDifference: " << callOrderDifference << ", levels: " << levelsInCurrent << std::endl;
              // for (unsigned i = 0; i < levelsInCurrent; i++) {
              //   auto v1 = currV[i];
              //   auto v2 = otherV[i];
              //   std::cout << "level: " << i << std::endl;
              //   std::cout << "  v1: ";
              //   for (auto & i : v1) std::cout << i << " ";
              //   std::cout << std::endl;
              //   std::cout << "  v2: ";
              //   for (auto & i : v2) std::cout << i << " ";
              //   std::cout << std::endl;
              // }
              mask = mask | uselessCon;
              continue;
            }
          }


          

          bool argEffectSimilarity = false;
          // If c2.smaller(c1) -- c2 is more specialized than c1
          if (other.smaller(currCon)) {

            auto otherEffectedArgs = other.getAffectedArguments();

            auto diffCon = other - currCon;
            auto affectedArgs = diffCon.getAffectedArguments();
            
            unsigned argDiff = 0;

            std::unordered_map<unsigned, std::vector<std::string>> otherArgMap;
            for (auto & data : simpleArgumentAnalysis[other]) {
              unsigned argIdx = data.first;
              auto calledFuns = data.second; 
              otherArgMap[argIdx] = calledFuns;
            }

            for (auto & ele : origArgMap) {
              unsigned argIdx = ele.first;
              std::set<std::string> v1(ele.second.begin(), ele.second.end());
              std::set<std::string> v2(otherArgMap[argIdx].begin(), otherArgMap[argIdx].end());

              // if the argument is not of interest, we might still want to see if things changed significantly
              // this might be mainly because of new type feedback, so increasing warmup for this case might be preferred

              bool argAvailableForComparisonInCurr = std::find(currEffectedArgs.begin(), currEffectedArgs.end(), argIdx) != currEffectedArgs.end();
              bool argAvailableForComparisonInOther = std::find(otherEffectedArgs.begin(), otherEffectedArgs.end(), argIdx) != otherEffectedArgs.end();
              if (std::find(affectedArgs.begin(), affectedArgs.end(), argIdx) == affectedArgs.end() && argAvailableForComparisonInCurr && argAvailableForComparisonInOther) {
                std::vector<std::string> diff;
                //no need to sort since it's already sorted
                std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
                  std::inserter(diff, diff.begin()));
                
                // for (auto ele : diff)
                //   candidateForLongerFeedback += RshBuiltinWeights::getWeight(ele);
                
                if (diff.size() > 0) {
                  std::cout << "candidate for longer type feedback wait: " << diff.size() << std::endl;
                }
              } else if (otherArgMap.find(argIdx) != otherArgMap.end()) {
                std::vector<std::string> diff;
                //no need to sort since it's already sorted
                std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
                  std::inserter(diff, diff.begin()));
                
                argDiff = diff.size();

                // for (auto ele : diff)
                //   argDifferenceWeight += RshBuiltinWeights::getWeight(ele);
              }
            }
            
            argEffectSimilarity = argDiff <= argumentEffectDifference;
          }

          // 0 - directly comparable && counter && argument
          // 1 - directly comparable && counter
          // 2 - roughly comparable && counter
          // 3 - counter 
          // 4 - counter || argument

          
          bool comparable = other.smaller(currCon);
          bool roughlyComparable = other.roughlySmaller(currCon);
          if (MASKING_RISK == 0) {
            if (comparable && counterSimilarity && argEffectSimilarity) mask = mask | uselessCon;
          } else if (MASKING_RISK == 1) {
            if (comparable && counterSimilarity) mask = mask | uselessCon;
          } else if (MASKING_RISK == 2) {
            if (roughlyComparable && counterSimilarity) mask = mask | uselessCon;
          } else if (MASKING_RISK == 3) {
            if (counterSimilarity) mask = mask | uselessCon;
          } else if (MASKING_RISK == 4) {
            if (counterSimilarity || argEffectSimilarity) mask = mask | uselessCon;
          }
        }
      }
      if (mask.toI() != 0) {
        std::cout << "  [MASK]: " << mask << std::endl;
        maskDataStream << currHastJson["hast"].get<std::string>() << "," << currOffset.key() << "," << mask.toI() << std::endl;
      }
    }

  }
  maskDataStream.close();
  return 0;
}
