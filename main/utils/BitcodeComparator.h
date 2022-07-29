#pragma once

#include <string>

class BitcodeComparator {
    public:
        static bool compareBitcodes(const std::string & path1, const std::string & path2);

        static unsigned STRICTNESS;
        static unsigned THRESHOLD_WEIGHT;
        static unsigned THRESHOLD_FUNCALL;
        static unsigned THRESHOLD_ARGEFFECT;
        
    private:
        static bool checkWeightSimilarity();
        static bool checkFunCallSimilarity();
        static bool checkArgEffectSimilarity();
};