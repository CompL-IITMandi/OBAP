#include "utils/ContextAnalysisComparison.h"
#include <string>
#include <algorithm>

#define DEBUG_COMPARISONS 0

static bool checkWeightSimilarity(rir::Context & c1, rir::Context & c2, std::unordered_map<rir::Context, unsigned> & weightAnalysis) {
  auto diff = weightAnalysis[c1] - weightAnalysis[c2];
  if (diff < 0) diff = -diff;
  return (diff <= ContextAnalysisComparison::THRESHOLD_WEIGHT);
}

static bool checkFunCallSimilarity(rir::Context & c1, rir::Context & c2, std::unordered_map<rir::Context, std::vector<std::set<std::string>>> & funCallBFData) {
  auto currV = funCallBFData[c1];
  auto otherV = funCallBFData[c2];
  auto levelsInCurrent = currV.size();
  auto levelsInOther = otherV.size();
  unsigned callOrderDifference = 0;
  
  if (levelsInCurrent == 0 && levelsInOther == 0) {
    // std::cout << "ZERO LEVELS IN BOTH" << std::endl;
    return true;
  }

  if (levelsInCurrent != levelsInOther) return false;

  for (unsigned i = 0; i < levelsInCurrent; i++) {
    auto v1 = currV[i];
    auto v2 = otherV[i];
    std::vector<std::string> diff1, diff2;
    //no need to sort since it's already sorted
    std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
      std::inserter(diff1, diff1.begin()));
    std::set_difference(v2.begin(), v2.end(), v1.begin(), v1.end(),
      std::inserter(diff2, diff2.begin()));

    if (diff1.size() > 0 || diff2.size() > 0) {
      callOrderDifference++;
    }
  }

  return ((callOrderDifference/levelsInCurrent) * 100) <= ContextAnalysisComparison::THRESHOLD_FUNCALL;
}

static void tokenize(std::set<std::string> & res, std::string s, std::string del = " ") {
  int start = 0;
  int end = s.find(del);
  while (end != -1) {
    res.insert(s.substr(start, end - start));
    // std::cout << s.substr(start, end - start) << std::endl;
    start = end + del.size();
    end = s.find(del, start);
  }
  res.insert(s.substr(start, end - start));
  // std::cout << s.substr(start, end - start);
}

static std::pair<bool, rir::Context> checkArgEffectSimilarity(rir::Context & c1, rir::Context & c2, rir::Context diffCon,  std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis) {
  rir::Context maskPart;

  auto c1AffectedArgs = c1.getAffectedArguments();
  auto c2AffectedArgs = c2.getAffectedArguments();
  auto diffAffectedArgs = diffCon.getAffectedArguments();

  #if DEBUG_COMPARISONS == 1
  std::cout << "== checkArgEffectSimilarity ==" << std::endl;
  std::cout << "c1(" << c1.toI() << "): " << c1 << std::endl;
  std::cout << "c2(" << c2.toI() << "): " << c2 << std::endl;
  std::cout << "diffCon: " << diffCon << std::endl;
  std::cout << "diffAffectedArgs: [";
  for (auto & ele : diffAffectedArgs) std::cout << ele << " ";
  std::cout << "]" << std::endl;
  #endif

  bool atleastOneSimilar = false;

  std::unordered_map<unsigned, std::vector<std::string>> c1ArgMap, c2ArgMap;
  // ArgIdx to [<set>, <set>]
  std::unordered_map<unsigned, std::vector<std::set<std::string>>> c1ArgSet, c2ArgSet;
  for (auto & data : simpleArgumentAnalysis[c1]) {
    unsigned argIdx = data.first;
    auto levelCalledFunData = data.second; 
    c1ArgMap[argIdx] = levelCalledFunData;
  }

  for (auto & data : simpleArgumentAnalysis[c2]) {
    unsigned argIdx = data.first;
    auto levelCalledFunData = data.second; 
    c2ArgMap[argIdx] = levelCalledFunData;
  }

  for (auto & ele : c1ArgMap) {
    std::vector<std::set<std::string>> v;
    for (auto & e : ele.second) {
      std::set<std::string> rrr;
      tokenize(rrr, e, "|");
      v.push_back(rrr);
    }
    c1ArgSet[ele.first] = v;
  }

  #if DEBUG_COMPARISONS == 1
  std::cout << "c1 set data" << std::endl;
  for (auto & ele : c1ArgSet) {
    unsigned level = 0;
    std::cout << "  " << ele.first << std::endl;
    for (auto & e : ele.second) {
      std::cout << "    Level " << level++ << std::endl;
      std::cout << "      ";
      for (auto & fun : e) {
        std::cout << fun << " ";
      }
      std::cout << std::endl;
    }
  }
  #endif

  for (auto & ele : c2ArgMap) {
    std::vector<std::set<std::string>> v;
    for (auto & e : ele.second) {
      std::set<std::string> rrr;
      tokenize(rrr, e, "|");
      v.push_back(rrr);
    }
    c2ArgSet[ele.first] = v;
  }

  #if DEBUG_COMPARISONS == 1
  std::cout << "c2 set data" << std::endl;
  for (auto & ele : c2ArgSet) {
    unsigned level = 0;
    std::cout << "  " << ele.first << std::endl;
    for (auto & e : ele.second) {
      std::cout << "    Level " << level++ << std::endl;
      std::cout << "      ";
      for (auto & fun : e) {
        std::cout << fun << " ";
      }
      std::cout << std::endl;
    }
  }
  #endif

  #if DEBUG_COMPARISONS == 1
  std::cout << "Diff" << std::endl;
  #endif

  for (auto & ele : diffAffectedArgs) {
    if (c1ArgSet.find(ele) != c1ArgSet.end() || c2ArgSet.find(ele) != c2ArgSet.end()) {
      #if DEBUG_COMPARISONS == 1
      std::cout << "  Comparing Arg " << ele << std::endl;
      #endif

      auto c1LevelData = c1ArgSet[ele];
      auto c2LevelData = c2ArgSet[ele];
      bool success = false;
      if (c1LevelData.size() == c2LevelData.size()) {
        for (unsigned i = 0; i < c1LevelData.size(); i++) {
          

          std::set<std::string> v1(c1LevelData[i].begin(), c1LevelData[i].end());
          std::set<std::string> v2(c2LevelData[i].begin(), c2LevelData[i].end());

          #if DEBUG_COMPARISONS == 1
          std::cout << "  Level " << i << std::endl;
          std::cout << "    v1: [";
          for (auto & e : v1) {
            std::cout << e << " ";
          }
          std::cout << "]" << std::endl;

          std::cout << "    v2: [";
          for (auto & e : v2) {
            std::cout << e << " ";
          }
          std::cout << "]" << std::endl;
          #endif

          std::vector<std::string> diff1;
          //no need to sort since it's already sorted
          std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
            std::inserter(diff1, diff1.begin()));

          std::vector<std::string> diff2;
          //no need to sort since it's already sorted
          std::set_difference(v2.begin(), v2.end(), v1.begin(), v1.end(),
            std::inserter(diff2, diff2.begin()));

          
          #if DEBUG_COMPARISONS == 1
          std::cout << "    diff: [";
          for (auto & ele : diff1) {
            std::cout << ele << " ";
          }
          for (auto & ele : diff2) {
            std::cout << ele << " ";
          }
          std::cout << "]" << std::endl;
          #endif

          if (diff1.size() == 0 && diff2.size() == 0) {
            success = true;
          }
        }
      }
      if (success) {
        maskPart = maskPart + diffCon.getArgRelatedAssumptions(ele);
        atleastOneSimilar = true;
        // std::cout << "  is similar" << std::endl;
      } else {
        // std::cout << "  not similar" << std::endl;
      }
    } else {
      std::cout << "[Warn] ArgEffect failed checking arguments at index " << ele << std::endl;
    }
  }

  // std::unordered_map<unsigned, std::vector<std::string>> c2ArgMap;
  // for (auto & data : simpleArgumentAnalysis[c2]) {
  //   unsigned argIdx = data.first;
  //   auto calledFuns = data.second; 
  //   c2ArgMap[argIdx] = calledFuns;
  //   allArgs.insert(argIdx);
  // }

  
  // for (auto & argIdx : allArgs) {
  //   bool argDataAvailableForC1 = c1ArgMap.find(argIdx) != c1ArgMap.end();
  //   bool argDataAvailableForC2 = c2ArgMap.find(argIdx) != c2ArgMap.end();

  //   bool diffedArg = diffAffectedArgs.find(argIdx) != diffAffectedArgs.end();

  //   if (argDataAvailableForC1 && argDataAvailableForC2) {
  //     if (diffedArg) {
  //       std::set<std::string> v1(c1ArgMap[argIdx].begin(), c1ArgMap[argIdx].end());
  //       std::set<std::string> v2(c2ArgMap[argIdx].begin(), c2ArgMap[argIdx].end());
  //       // If this argument remains same, this part of the context can be masked
  //       std::vector<std::string> diff1;
  //       //no need to sort since it's already sorted
  //       std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(),
  //         std::inserter(diff1, diff1.begin()));
        
  //       std::vector<std::string> diff2;
  //       //no need to sort since it's already sorted
  //       std::set_difference(v2.begin(), v2.end(), v1.begin(), v1.end(),
  //         std::inserter(diff2, diff2.begin()));
        
  //       #if DEBUG_COMPARISONS == 1
  //       std::cout << "v1: [";
  //       for (auto & ele : v1) std::cout << ele << " ";
  //       std::cout << "]" << std::endl;

  //       std::cout << "v2: [";
  //       for (auto & ele : v2) std::cout << ele << " ";
  //       std::cout << "]" << std::endl;

  //       std::cout << "diff1: [";
  //       for (auto & ele : diff1) std::cout << ele << " ";
  //       std::cout << "]" << std::endl;

  //       std::cout << "diff2: [";
  //       for (auto & ele : diff2) std::cout << ele << " ";
  //       std::cout << "]" << std::endl;
  //       #endif

  //       if (diff1.size() == 0 && diff2.size() == 0) {
  //         #if DEBUG_COMPARISONS == 1
  //         // std::cout << "Arg is same: " << argIdx << std::endl;
  //         #endif
  //         maskPart = maskPart + diffCon.getArgRelatedAssumptions(argIdx);
  //         atleastOneSimilar = true;
  //       } else {
  //         #if DEBUG_COMPARISONS == 1
  //         // std::cout << "Arg is different: " << argIdx << std::endl;
  //         #endif
  //       }
  //     }
  //   }
  // }
  return std::pair<bool, rir::Context>(atleastOneSimilar, maskPart);
}

rir::Context ContextAnalysisComparison::getDiff() {
  switch (type) {
    case ComparisonType::STRICT: {
      return (c2 - c1);
    }
    case ComparisonType::ROUGH_EQ: 
    case ComparisonType::ROUGH_NEQ: {
      auto res = ((c2 - c1) + (c1 - c2));
      return res.getArgRealtedContext();
    }
  }
  return rir::Context(0ul);
}

bool ContextAnalysisComparison::safeToRemoveContext(const rir::Context & mask) {
  if (type == ComparisonType::STRICT) {
    if (mask.toI() == getDiff().toI()) {
      return true;
    }
  }
  return false;
}

rir::Context ContextAnalysisComparison::getMask(
  std::unordered_map<rir::Context, unsigned> & weightAnalysis,
  std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis,
  std::unordered_map<rir::Context, std::vector<std::set<std::string>>> & funCallBFData
  ) {
  rir::Context mask;

  static bool ANALYSIS_WT = getenv("ANALYSIS_WT") ? 1 : 0;
  static bool ANALYSIS_FUNCALL_BF = getenv("ANALYSIS_FUNCALL_BF") ? 1 : 0;
  static bool ANALYSIS_ARGEFFECT = getenv("ANALYSIS_ARGEFFECT") ? 1 : 0;

  static bool ANDALL = getenv("ANDALL") ? 1 : 0;

  // if (type == ComparisonType::STRICT) {
  //   std::cout << "STRICT COMPARISON" << std::endl;
  // } else {
  //   std::cout << "ROUGH COMPARISON" << std::endl;
  // }

  if (ANDALL) {
    bool weightSimilarity = checkWeightSimilarity(c1, c2, weightAnalysis);
    bool funCallSimilarity = checkFunCallSimilarity(c1, c2, funCallBFData);
    auto argEffectSimilarity = checkArgEffectSimilarity(c1, c2, getDiff(), simpleArgumentAnalysis);
    if (weightSimilarity && funCallSimilarity && argEffectSimilarity.first) {
      mask = getDiff();
    }

  } else {

    if (ANALYSIS_WT == 1) {
      bool weightSimilarity = checkWeightSimilarity(c1, c2, weightAnalysis);
      if (weightSimilarity) {
        mask = getDiff();
      }
    }

    if (ANALYSIS_FUNCALL_BF == 1) {
      bool funCallSimilarity = checkFunCallSimilarity(c1, c2, funCallBFData);
      if (funCallSimilarity) {
        mask = mask + getDiff();
      }
    }

    if (ANALYSIS_ARGEFFECT == 1) {
      auto argEffectSimilarity = checkArgEffectSimilarity(c1, c2, getDiff(), simpleArgumentAnalysis);
      if (argEffectSimilarity.first) {
        mask = mask + argEffectSimilarity.second;
      }
    }
  }
  

  return mask;
}

unsigned ContextAnalysisComparison::STRICTNESS = getenv("STRICTNESS") ? std::stoi(getenv("STRICTNESS")) : 2;
unsigned ContextAnalysisComparison::THRESHOLD_WEIGHT = getenv("THRESHOLD_WEIGHT") ? std::stoi(getenv("THRESHOLD_WEIGHT")) : 5;
unsigned ContextAnalysisComparison::THRESHOLD_FUNCALL = getenv("THRESHOLD_FUNCALL") ? std::stoi(getenv("THRESHOLD_FUNCALL")) : 25;
unsigned ContextAnalysisComparison::THRESHOLD_ARGEFFECT = getenv("THRESHOLD_ARGEFFECT") ? std::stoi(getenv("THRESHOLD_ARGEFFECT")) : 10;
