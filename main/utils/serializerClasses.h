#pragma once

class OffsetsContainer {
public:
    OffsetsContainer(SEXP c) : _container(c) {}

    int getNumOffsets() {
        REnvHandler offsetMap(_container);
        return offsetMap.size();
    }

    // iterator
    void iterate(const std::function< void(SEXP, SEXP) >& callback) {
      rir::Protect protecc;
      SEXP offsetBindings = R_lsInternal(_container, (Rboolean) false);
      protecc(offsetBindings);
      for (int i = 0; i < Rf_length(offsetBindings); i++) {
        SEXP key = Rf_install(CHAR(STRING_ELT(offsetBindings, i)));
        SEXP binding = Rf_findVarInFrame(_container, key);
        if (binding == R_UnboundValue) continue;
        callback(key, binding);
      }
    }

private:
    SEXP _container;
};