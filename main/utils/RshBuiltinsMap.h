#ifndef RBIM_H
#define RBIM_H


class RshBuiltinWeights {
public:
  static std::unordered_map<std::string, unsigned> weightMap;

  static void init() {
    weightMap["forcePromise"] = 10;
    weightMap["consNr"] = 1;
    weightMap["createBindingCellImpl"] = 1;
    weightMap["createMissingBindingCell"] = 1;
    weightMap["createEnvironment"] = 5;
    weightMap["createStubEnvironment"] = 2;
    weightMap["materializeEnvironment"] = 7;
    weightMap["ldvarForUpdate"] = 3;

    weightMap["ldvar"] = 2;
    weightMap["ldvarGlobal"] = 2;
    weightMap["ldvarCacheMiss"] = 2;
    weightMap["stvarSuper"] = 2;
    weightMap["stvar"] = 1;
    weightMap["stvari"] = 1;
    weightMap["stvarr"] = 1;
    weightMap["starg"] = 1;
    weightMap["setCar"] = 1;
    weightMap["setCdr"] = 1;
    weightMap["setTag"] = 1;
    weightMap["externalsxpSetEntry"] = 1;
    weightMap["defvar"] = 2;
    weightMap["ldfun"] = 2;
    weightMap["chkfun"] = 1;
    weightMap["warn"] = 1;
    weightMap["error"] = 1;
    weightMap["callBuiltin"] = 10;
    weightMap["call"] = 10;
    weightMap["namedCall"] = 10;
    weightMap["dotsCall"] = 10;
    weightMap["createPromise"] = 3;
    weightMap["createPromiseNoEnvEager"] = 3;
    weightMap["createPromiseNoEnv"] = 3;
    weightMap["createPromiseEager"] = 3;


    weightMap["createClosure"] = 2;
    weightMap["newIntFromReal"] = 1;
    weightMap["newRealFromInt"] = 1;
    weightMap["newInt"] = 1;
    weightMap["newIntDebug"] = 1;

    weightMap["newReal"] = 1;
    weightMap["unopEnv"] = 1;
    weightMap["unop"] = 1;
    weightMap["notEnv"] = 1;

    weightMap["not"] = 1;
    weightMap["binopEnv"] = 1;
    weightMap["binop"] = 1;
    weightMap["colon"] = 1;

    weightMap["isMissing"] = 1;
    weightMap["isFactor"] = 1;
    weightMap["asSwitchIdx"] = 1;
    weightMap["checkTrueFalse"] = 1;

    weightMap["aslogical"] = 1;

    weightMap["length"] = 1;
    weightMap["recordTypefeedback"] = 1;
    weightMap["deopt"] = 1;
    weightMap["deoptPool"] = 1;
    weightMap["assertFail"] = 1;
    weightMap["printValue"] = 1;

    weightMap["extract1_1D"] = 10;
    weightMap["extract2_1D"] = 10;
    weightMap["extract2_1Di"] = 10;
    weightMap["extract2_1Dr"] = 10;
    weightMap["extract1_2D"] = 10;

    weightMap["extract1_3D"] = 10;
    weightMap["extract2_2D"] = 10;
    weightMap["extract2_2Dii"] = 10;
    weightMap["extract2_2Drr"] = 10;
    weightMap["nativeCallTrampoline"] = 7;
    weightMap["subassign1_1D"] = 10;
    weightMap["setVecElt"] = 2;
    weightMap["subassign2_1D"] = 10;
    weightMap["subassign2_1D_ii"] = 10;
    weightMap["subassign2_1D_rr"] = 10;
    weightMap["subassign2_1D_ri"] = 10;
    weightMap["subassign2_1D_ir"] = 10;
    weightMap["subassign1_22"] = 10;
    weightMap["subassign1_3D"] = 10;
    weightMap["subassign2_2D"] = 10;
    weightMap["subassign2_2Diii"] = 10;
    weightMap["subassign2_2Drrr"] = 10;

    weightMap["subassign2_2Drr1"] = 10;
    weightMap["subassign2_2Diir"] = 10;
    weightMap["toForSeq"] = 1;
    weightMap["initClosureContext"] = 2;
    weightMap["endClosureContext"] = 2;
    weightMap["ncols"] = 1;
    weightMap["nrows"] = 1;
    weightMap["makeVector"] = 1;
    weightMap["prodr"] = 1;
    weightMap["sumr"] = 1;
    weightMap["colonInputEffects"] = 1;
    weightMap["colonCastLhs"] = 1;
    weightMap["colonCastRhs"] = 1;
    weightMap["names"] = 1;
    weightMap["setNames"] = 1;
    weightMap["xlength"] = 2;
    weightMap["getAttrib"] = 1;
    weightMap["nonLocalReturn"] = 1;
    weightMap["cksEq"] = 1;
    weightMap["checkType"] = 1;
    weightMap["shallowDuplicate"] = 5;
    weightMap["deoptChaosTrigger"] = 2;

    weightMap["llDebugMsg"] = 0;
    weightMap["sigsetjmp"] = 1;
    weightMap["__sigsetjmp"] = 1;
    

  }

  static unsigned getWeight(const std::string & key) {
    if (weightMap.find(key) == weightMap.end()) return 0;
    return weightMap[key];
  }
};
#endif