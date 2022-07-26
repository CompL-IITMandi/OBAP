#pragma once
#include <array>
#include <cstdint>
#include <iostream>

namespace rir {

inline bool fastVeceltOk(SEXP vec) {
    return !isObject(vec) &&
           (ATTRIB(vec) == R_NilValue || (TAG(ATTRIB(vec)) == R_DimSymbol &&
                                          CDR(ATTRIB(vec)) == R_NilValue));
}

struct ObservedValues {

    enum StateBeforeLastForce {
        unknown,
        value,
        evaluatedPromise,
        promise,
    };

    static constexpr unsigned MaxTypes = 3;
    uint8_t numTypes : 2;
    uint8_t stateBeforeLastForce : 2;
    uint8_t notScalar : 1;
    uint8_t attribs : 1;
    uint8_t object : 1;
    uint8_t notFastVecelt : 1;

    std::array<uint8_t, MaxTypes> seen;

    ObservedValues() {
        // implicitly happens when writing bytecode stream...
        memset(this, 0, sizeof(ObservedValues));
    }

    void reset() { *this = ObservedValues(); }

    void print(std::ostream& out) const {
        if (numTypes) {
            for (size_t i = 0; i < numTypes; ++i) {
                out << Rf_type2char(seen[i]);
                if (i != (unsigned)numTypes - 1)
                    out << ", ";
            }
            out << " (" << (object ? "o" : "") << (attribs ? "a" : "")
                << (notFastVecelt ? "v" : "") << (!notScalar ? "s" : "") << ")";
            if (stateBeforeLastForce !=
                ObservedValues::StateBeforeLastForce::unknown) {
                out << " | "
                    << ((stateBeforeLastForce ==
                         ObservedValues::StateBeforeLastForce::value)
                            ? "value"
                            : (stateBeforeLastForce ==
                               ObservedValues::StateBeforeLastForce::
                                   evaluatedPromise)
                                  ? "evaluatedPromise"
                                  : "promise");
            }
        } else {
            out << "<?>";
        }
    };

    inline void record(SEXP e) {

        // Set attribs flag for every object even if the SEXP does  not
        // have attributes. The assumption used to be that e having no
        // attributes implies that it is not an object, but this is not
        // the case in some very specific cases:
        //     > df <- data.frame(x=ts(c(41,42,43)), y=c(61,62,63))
        //     > mf <- model.frame(df)
        //     > .Internal(inspect(mf[["x"]]))
        //     @56546cb06390 14 REALSXP g0c3 [OBJ,NAM(2)] (len=3, tl=0) 41,42,43

        notScalar = notScalar || XLENGTH(e) != 1;
        object = object || isObject(e);
        attribs = attribs || object || ATTRIB(e) != R_NilValue;
        notFastVecelt = notFastVecelt || !fastVeceltOk(e);

        uint8_t type = TYPEOF(e);
        if (numTypes < MaxTypes) {
            int i = 0;
            for (; i < numTypes; ++i) {
                if (seen[i] == type)
                    break;
            }
            if (i == numTypes)
                seen[numTypes++] = type;
        }
    }
};
static_assert(sizeof(ObservedValues) == sizeof(uint32_t),
              "Size needs to fit inside a record_ bc immediate args");



} // namespace rir
