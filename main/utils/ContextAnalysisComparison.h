#ifndef CON_AC_H
#define CON_AC_H

#include "rir/Context.h"
#include "utils/iter.h"

class ContextAnalysisComparison {
  private:
    Context c1, c2;
    ComparisonType type;
  public:
    static unsigned AGRESSION;
    static unsigned THRESHOLD_WEIGHT;
    static unsigned THRESHOLD_FUNCALL;
    static unsigned THRESHOLD_ARGEFFECT;
    ContextAnalysisComparison(Context con1, Context con2, ComparisonType t) : c1(con1), c2(con2), type(t) {} 

    Context getMask(
      std::unordered_map<Context, unsigned> & weightAnalysis,
      std::unordered_map<Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis,
      std::unordered_map<Context, std::vector<std::set<std::string>>> & funCallBFData
      );
};

#endif