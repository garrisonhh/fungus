#include <string.h>

#include "utils.h"

bool global_error = false;

void names_to_lower(char (*dst)[MAX_NAME_LEN], char **src, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        size_t len = strlen(src[i]);

#ifdef DEBUG
        if (len >= MAX_NAME_LEN)
            fungus_panic("name '%s' is longer than MAX_NAME_LEN.", src);
#endif

        for (size_t j = 0; j < len; ++j) {
            char c = src[i][j];

            if (c >= 'A' && c <= 'Z')
                dst[i][j] = c - ('A' - 'a');
            else
                dst[i][j] = c;
        }
    }
}

// TODO deprecate
void fungus_error(const char *msg, ...) {
    va_list args;

    fprintf(stderr, "[" TC_RED "ERROR (TODO: USE FILE ERROR)" TC_RESET "]: ");

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void fungus_panic(const char *msg, ...) {
    va_list args;

    fprintf(stderr,
            "[" TC_BLINK TC_YELLOW "FUNGUS" TC_RED "PANIC" TC_RESET "]: ");

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

hash_t fnv_hash_start(void) {
    return FNV_BASIS;
}

hash_t fnv_hash_next(hash_t hash, char byte) {
    hash ^= (hash_t)byte;
    hash *= FNV_PRIME;

    return hash;
}
