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

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

/*
 * for comparison functions:
 * cmp(a, b) > 0 // a > b
 * cmp(a, b) == 0 // a == b
 * cmp(a, b) < 0 // a < b
 */
typedef enum Comparison { LT = -1, EQ = 0, GT = 1 } Comparison;

// table names =================================================================

// when using table macros I usually like to make name arrays nicer looking

#define MAX_NAME_LEN 32
void names_to_lower(char (*dst)[MAX_NAME_LEN], char **src, size_t len);

// term codes ==================================================================

#define TC_RESET   "\x1b[0m"
#define TC_BOLD    "\x1b[1m"
#define TC_DIM     "\x1b[2m"
#define TC_BLINK   "\x1b[5m"
#define TC_INVERSE "\x1b[7m"

#define TC_BLACK   "\x1b[30m"
#define TC_RED     "\x1b[31m"
#define TC_GREEN   "\x1b[32m"
#define TC_YELLOW  "\x1b[33m"
#define TC_BLUE    "\x1b[34m"
#define TC_MAGENTA "\x1b[35m"
#define TC_CYAN    "\x1b[36m"
#define TC_WHITE   "\x1b[37m"

#define TC_GRAY    "\x1b[90m"

// errors ======================================================================

// set to true if error has happened
// TODO get rid of this? what is a better error system?
extern bool global_error;

void fungus_error(const char *msg, ...);
noreturn void fungus_panic(const char *msg, ...);

// hashing =====================================================================

typedef uint64_t hash_t;

hash_t fnv_hash(const char *data, size_t nbytes);

// hash a byte at a time
hash_t fnv_hash_start(void);
hash_t fnv_hash_next(hash_t, char byte);

#endif
