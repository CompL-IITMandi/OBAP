#include "utils/TVGraph.h"

unsigned int TVGraph::MAX_SLOTS_SIZE = 0;
int TVGraph::indIdx = 30;

std::unordered_map<SEXP, unsigned int> TVGraph::feedbackIndirections;
