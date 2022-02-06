#ifndef WORDS_H
#define WORDS_H

#include <stddef.h>
#include <string.h>

#include "data.h"
#include "utils.h"

/*
 * views are a classic string slice. words are a pre-hashed string slice.
 */

typedef struct View {
    const char *str;
    size_t len;
} View;

typedef union Word {
    struct {
        const char *str;
        size_t len;
        hash_t hash;
    };
    View as_view;
} Word;

Word Word_new(const char *str, size_t len);
Word *Word_copy_of(Word *src, Bump *pool);

#define WORD(STR) Word_new(STR, strlen(STR))

bool Word_eq(Word *a, Word *b);
bool Word_eq_view(Word *a, View *b);

#endif
