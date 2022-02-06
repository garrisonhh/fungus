#ifndef PRECEDENCE_H
#define PRECEDENCE_H

#include "words.h"

typedef unsigned PrecId;

PrecId prec_unique_id(Word *name);
int prec_cmp(PrecId a, PrecId b); // > 0 means a > b, < 0 means a < b

#endif
