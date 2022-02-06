#ifndef RULES_H
#define RULES_H

#include "types.h"
#include "precedence.h"

typedef struct SyntaxPattern {
    // if ty or mty is NONE, pattern matches any ty or mty
    Type ty;
    MetaType mty;
} Pattern;

typedef struct SyntaxRule {
    Pattern *pattern;
    size_t len;
    PrecId prec;

    Type ty;
    MetaType mty;

    // TODO some understanding of what this rule represents?
} Rule;

#endif
