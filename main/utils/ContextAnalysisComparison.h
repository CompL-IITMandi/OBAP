#ifndef CON_AC_H
#define CON_AC_H

#include "runtime/Context.h"
#include "utils/iter.h"

class ContextAnalysisComparison {
  private:
    rir::Context c1, c2;
    ComparisonType type;
  public:
    static unsigned STRICTNESS;
    static unsigned THRESHOLD_WEIGHT;
    static unsigned THRESHOLD_FUNCALL;
    static unsigned THRESHOLD_ARGEFFECT;
    ContextAnalysisComparison(rir::Context con1, rir::Context con2, ComparisonType t) : c1(con1), c2(con2), type(t) {}

    rir::Context getDiff();

    bool safeToRemoveContext(const rir::Context & mask);

    rir::Context getMask(
      std::unordered_map<rir::Context, unsigned> & weightAnalysis,
      std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis,
      std::unordered_map<rir::Context, std::vector<std::set<std::string>>> & funCallBFData
      );
};

#endif