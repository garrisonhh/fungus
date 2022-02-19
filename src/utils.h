#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>

// random useful stuff =========================================================

#define CAT2(A, B) A##B
#define CAT(A, B) CAT2(A, B)

#define STRINGIFY2(A) #A
#define STRINGIFY(A) STRINGIFY2(A)

#define ARRAY_SIZE(ARR) (sizeof(ARR) / sizeof(ARR[0]))

/*
 * for comparison functions:
 * cmp(a, b) > 0 // a > b
 * cmp(a, b) == 0 // a == b
 * cmp(a, b) < 0 // a < b
 */
typedef enum Comparison { LT = -1, EQ = 0, GT = 1 } Comparison;

// term codes ==================================================================

#define TERM_TABLE\
    X(RESET,    0)\
    \
    X(BLINK,    5)\
    X(INVERSE,  7)\
    \
    X(BLACK,    30)\
    X(RED,      31)\
    X(GREEN,    32)\
    X(YELLOW,   33)\
    X(BLUE,     34)\
    X(MAGENTA,  35)\
    X(CYAN,     36)\
    X(WHITE,    37)\

#define X(NAME, CODE) TERM_##NAME,
typedef enum TermFormat { TERM_TABLE } TermFmt;
#undef X

void term_format(TermFmt fmt);
void term_fformat(FILE *, TermFmt fmt);

// errors ======================================================================

// set to true if error has happened
extern bool global_error;

void fungus_error(const char *msg, ...);
noreturn void fungus_panic(const char *msg, ...);

// hashing =====================================================================

typedef uint64_t hash_t;

hash_t fnv_hash(const char *data, size_t nbytes);

#endif
