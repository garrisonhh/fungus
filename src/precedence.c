#include "precedence.h"

PrecId prec_unique_id(Word *name) {
    static PrecId prec_counter = 0;

    return prec_counter++;
}

int prec_cmp(PrecId a, PrecId b) {
    return b - a;
}

