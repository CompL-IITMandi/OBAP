#pragma once
#include <iostream>
#include <string>
#include <unistd.h>

#include "Rinternals.h"

struct FunctionSignature {
    enum class Environment {
        CallerProvided,
        CalleeCreated,
    };

    enum class OptimizationLevel {
        Baseline,
        Optimized,
        Contextual,
    };

    void pushFormal(SEXP arg, SEXP name) {
        if (arg != R_MissingArg)
            hasDefaultArgs = true;
        if (name == R_DotsSymbol) {
            hasDotsFormals = true;
            dotsPosition = numArguments;
        }
        numArguments++;
    }

    void print(std::ostream& out = std::cout) const {
        if (optimization != OptimizationLevel::Baseline)
            out << "optimized code ";
        if (envCreation == Environment::CallerProvided)
            out << "needsEnv ";
    }

  public:
    FunctionSignature() = delete;
    FunctionSignature(Environment envCreation, OptimizationLevel optimization)
        : envCreation(envCreation), optimization(optimization) {}

    size_t formalNargs() const { return numArguments; }

    const Environment envCreation;
    const OptimizationLevel optimization;
    unsigned numArguments = 0;
    bool hasDotsFormals = false;
    bool hasDefaultArgs = false;
    size_t dotsPosition = -1;
};