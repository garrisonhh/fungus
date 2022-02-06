#include "utils.h"

#define X(NAME, CODE) CODE,
static const int TERM_CODES[] = { TERM_TABLE };
#undef X

bool global_error = false;

void term_fformat(FILE *fp, TermFmt fmt) {
    fflush(fp);
    fprintf(fp, "\x1b[%dm", TERM_CODES[fmt]);
}

void term_format(TermFmt fmt) {
    term_fformat(stdout, fmt);
}

void fungus_error(const char *msg, ...) {
    va_list args;

    fprintf(stderr, "[");
    term_fformat(stderr, TERM_RED);
    fprintf(stderr, "ERROR");
    term_fformat(stderr, TERM_RESET);
    fprintf(stderr, "]: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void fungus_panic(const char *msg, ...) {
    va_list args;

    fprintf(stderr, "[");
    term_fformat(stderr, TERM_BLINK);
    term_fformat(stderr, TERM_YELLOW);
    fprintf(stderr, "FUNGUS");
    term_fformat(stderr, TERM_RED);
    fprintf(stderr, "PANIC");
    term_fformat(stderr, TERM_RESET);
    fprintf(stderr, "]: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");

    exit(-1);
}

#define FNV_PRIME 1099511628211ull
#define FNV_BASIS 14695981039346656037ull

// fnv-1a
hash_t fnv_hash(const char *data, size_t nbytes) {
    hash_t hash = FNV_BASIS;

    for (size_t i = 0; i < nbytes; ++i) {
        hash ^= (hash_t)data[i];
        hash *= FNV_PRIME;
    }

    return hash;
}
