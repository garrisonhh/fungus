#ifndef DATA_H
#define DATA_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "utils.h"

// dyn array ===================================================================

typedef struct Vector {
    void **data;
    size_t len, cap;
} Vec;

Vec Vec_new(void);
void Vec_del(Vec *);

void **Vec_alloc(Vec *);
void Vec_clear(Vec *);
void Vec_push(Vec *, const void *item);
void Vec_ordered_insert(Vec *, size_t idx, const void *item);
void Vec_qsort(Vec *, int (*cmp)(const void *, const void *));

// bump memory =================================================================

typedef struct Bump {
    Vec pages, lost_n_found;
    char *page;
    size_t bump;
} Bump;

Bump Bump_new(void);
void Bump_del(Bump *);

void *Bump_alloc(Bump *, size_t nbytes);

void Bump_clear(Bump *);

// views + words ===============================================================
// views are a classic string slice. words are a pre-hashed string slice.

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

bool Word_eq(const Word *a, const Word *b);
bool Word_eq_view(const Word *a, const View *b);

// ordered Word -> id hashmap ==================================================

// TODO replace with a faster flat (non-node-based) version at some point

typedef struct IdMapNode {
    size_t data;
} IdMapNode;

typedef struct IdMap {
    IdMapNode *nodes;
    size_t size, cap;
} IdMap;

#endif
