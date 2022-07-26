#include "Context.h"

namespace rir {


std::ostream& operator<<(std::ostream& out, Assumption a) {
    switch (a) {
    case Assumption::NoExplicitlyMissingArgs:
        out << "!ExpMi";
        break;
    case Assumption::CorrectOrderOfArguments:
        out << "CorrOrd";
        break;
    case Assumption::StaticallyArgmatched:
        out << "Argmatch";
        break;
    case Assumption::NotTooManyArguments:
        out << "!TMany";
        break;
    }
    return out;
};

std::ostream& operator<<(std::ostream& out, TypeAssumption a) {
    switch (a) {
#define TYPE_ASSUMPTIONS(Type, Msg)                                            \
    case TypeAssumption::Arg0Is##Type##_:                                      \
        out << Msg << "0";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg1Is##Type##_:                                      \
        out << Msg << "1";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg2Is##Type##_:                                      \
        out << Msg << "2";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg3Is##Type##_:                                      \
        out << Msg << "3";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg4Is##Type##_:                                      \
        out << Msg << "4";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg5Is##Type##_:                                      \
        out << Msg << "5";                                                     \
        break;                                                                 \

        TYPE_ASSUMPTIONS(Eager, "Eager");
        TYPE_ASSUMPTIONS(NotObj, "!Obj");
        TYPE_ASSUMPTIONS(SimpleInt, "SimpleInt");
        TYPE_ASSUMPTIONS(SimpleReal, "SimpleReal");
        TYPE_ASSUMPTIONS(NonRefl, "NonRefl");
    }
    return out;
};

std::ostream& operator<<(std::ostream& out, const Context& a) {
    for (auto i = a.flags.begin(); i != a.flags.end(); ++i) {
        out << *i;
        if (i + 1 != a.flags.end())
            out << ",";
    }
    if (!a.typeFlags.empty())
        out << ";";
    for (auto i = a.typeFlags.begin(); i != a.typeFlags.end(); ++i) {
        out << *i;
        if (i + 1 != a.typeFlags.end())
            out << ",";
    }
    if (a.missing > 0)
        out << " miss: " << (int)a.missing;
    return out;
}

} // namespace rir
