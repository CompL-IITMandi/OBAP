#pragma once
#include <iostream>
#include "Rinternals.h"
#include "R/Protect.h"

void printSpace(const unsigned int & space, std::ostream& out = std::cout);

template <typename T>
static void addTToContainer(SEXP container, const int & index, T val) {
    rir::Protect protecc;
    SEXP store;
    protecc(store = Rf_allocVector(RAWSXP, sizeof(T)));
    T * tmp = (T *) DATAPTR(store);
    *tmp = val;

    SET_VECTOR_ELT(container, index, store);
}

template <typename T>
static T getTFromContainer(SEXP container, const int & index) {
    SEXP dataContainer = VECTOR_ELT(container, index);
    T* res = (T *) DATAPTR(dataContainer);
    return *res;
}