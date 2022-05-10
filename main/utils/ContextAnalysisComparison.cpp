#include "utils/ContextAnalysisComparison.h"
#include <string>
//  AGRESSION 0
//    STRICT
//      weightSimilarity || argumentEffectSimilarity || funCallSimilarity 
//    ROUGH
//      weightSimilarity || funCallSimilarity
//    DIFFZEROMISS
//      weightSimilarity || argumentEffectSimilarity || funCallSimilarity
//    DIFFSAMEMISS
//      weightSimilarity || argumentEffectSimilarity || funCallSimilarity
//    DIFFDIFFMISS
//      weightSimilarity || funCallSimilarity

static bool checkWeightSimilarity(Context & c1, Context & c2, std::unordered_map<Context, unsigned> & weightAnalysis) {
  auto diff = weightAnalysis[c1] - weightAnalysis[c2];
  if (diff < 0) diff = -diff;
  return (diff <= ContextAnalysisComparison::THRESHOLD_WEIGHT);
}

static bool checkFunCallSimilarity(Context & c1, Context & c2, std::unordered_map<Context, std::vector<std::set<std::string>>> & funCallBFData) {
  auto currV = funCallBFData[c1];
  auto otherV = funCallBFData[c2];
  auto levelsInCurrent = currV.size();
  auto levelsInOther = otherV.size();
  unsigned callOrderDifference = 0;
  
  if (levelsInCurrent == 0 && levelsInOther == 0) {
    std::cout << "ZERO LEVELS IN BOTH" << std::endl;
    return true;
  }

  if (levelsInCurrent != levelsInOther) return false;

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

  return ((callOrderDifference/levelsInCurrent) * 100) <= ContextAnalysisComparison::THRESHOLD_FUNCALL;
}

static std::pair<bool, Context> checkArgEffectSimilarity(Context & c1, Context & c2, std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis) {
  Context maskPart;

  auto c1AffectedArgs = c1.getAffectedArguments();
  auto c2AffectedArgs = c2.getAffectedArguments();
  auto diffCon = c2 - c1;
  auto diffAffectedArgs = diffCon.getAffectedArguments();

  bool atleastOneSimilar = false;

  std::set<unsigned> allArgs;

  std::unordered_map<unsigned, std::vector<std::string>> c1ArgMap;
  for (auto & data : simpleArgumentAnalysis[c1]) {
    unsigned argIdx = data.first;
    auto calledFuns = data.second; 
    c1ArgMap[argIdx] = calledFuns;
    allArgs.insert(argIdx);
  }

  std::unordered_map<unsigned, std::vector<std::string>> c2ArgMap;
  for (auto & data : simpleArgumentAnalysis[c2]) {
    unsigned argIdx = data.first;
    auto calledFuns = data.second; 
    c2ArgMap[argIdx] = calledFuns;
    allArgs.insert(argIdx);
  }

  
  for (auto & argIdx : allArgs) {
    bool argDataAvailableForC1 = c1ArgMap.find(argIdx) != c1ArgMap.end();
    bool argDataAvailableForC2 = c2ArgMap.find(argIdx) != c2ArgMap.end();

    bool diffedArg = diffAffectedArgs.find(argIdx) != diffAffectedArgs.end();

    if (argDataAvailableForC1 && argDataAvailableForC2) {
      if (diffedArg) {
        std::set<std::string> v1(c1ArgMap[argIdx].begin(), c1ArgMap[argIdx].end());
        std::set<std::string> v2(c2ArgMap[argIdx].begin(), c2ArgMap[argIdx].end());
        // If this argument remains same, this part of the context can be masked
        std::vector<std::string> diff;
        //no need to sort since it's already sorted
        std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
          std::inserter(diff, diff.begin()));

        if (diff.size() == 0) {
          maskPart = maskPart + diffCon.getArgRelatedAssumptions(argIdx);
          atleastOneSimilar = true;
        }
      }
    }
  }
  return std::pair<bool, Context>(atleastOneSimilar, maskPart);
}

Context ContextAnalysisComparison::getMask(
  std::unordered_map<Context, unsigned> & weightAnalysis,
  std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis,
  std::unordered_map<Context, std::vector<std::set<std::string>>> & funCallBFData
  ) {
  Context mask;
  if (AGRESSION == 0) {
    bool weightSimilarity = checkWeightSimilarity(c1, c2, weightAnalysis);
    bool funCallSimilarity = checkFunCallSimilarity(c1, c2, funCallBFData);
    auto argEffectSimilarity = checkArgEffectSimilarity(c1, c2, simpleArgumentAnalysis);
    switch (type) {
      case ComparisonType::STRICT: {
        if (weightSimilarity || funCallSimilarity || argEffectSimilarity.first) {
          mask = mask + (c2 - c1);
        }
        break;
      }
      case ComparisonType::ROUGH: {
        if (weightSimilarity || funCallSimilarity) {
          mask = mask + (c2 - c1);
        }
        break;
      }
      case ComparisonType::DIFFZEROMISS: {
        if (weightSimilarity || funCallSimilarity || argEffectSimilarity.first) {
          mask = mask + (c2 - c1);
        }
        break;
      }
      case ComparisonType::DIFFSAMEMISS: {
        if (weightSimilarity || funCallSimilarity || argEffectSimilarity.first) {
          mask = mask + (c2 - c1);
        }
        break;
      }
      case ComparisonType::DIFFDIFFMISS: {
        if (weightSimilarity || funCallSimilarity) {
          mask = mask + (c2 - c1);
        }
        break;
      }
    }
  }
  return mask;
}

unsigned ContextAnalysisComparison::AGRESSION = getenv("AGRESSION") ? std::stoi(getenv("AGRESSION")) : 0;
unsigned ContextAnalysisComparison::THRESHOLD_WEIGHT = getenv("THRESHOLD_WEIGHT") ? std::stoi(getenv("THRESHOLD_WEIGHT")) : 5;
unsigned ContextAnalysisComparison::THRESHOLD_FUNCALL = getenv("THRESHOLD_FUNCALL") ? std::stoi(getenv("THRESHOLD_FUNCALL")) : 25;
unsigned ContextAnalysisComparison::THRESHOLD_ARGEFFECT = getenv("THRESHOLD_ARGEFFECT") ? std::stoi(getenv("THRESHOLD_ARGEFFECT")) : 10;
