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
void *Vec_pop(Vec *);
void Vec_ordered_insert(Vec *, size_t idx, const void *item);
void Vec_qsort(Vec *, int (*cmp)(const void *, const void *));

// bump memory =================================================================

typedef struct Bump {
    Vec pages, lost_n_found;
    char *page;
    size_t bump;

#ifdef DEBUG
    size_t total, allocated;
#endif
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

bool View_eq(const View *a, const View *b);
// returns '\0' if past end of view
char View_get(const View *, size_t index);

typedef union Word {
    struct {
        const char *str;
        size_t len;
        hash_t hash;
    };
    View as_view;
} Word;

#define WORD(STR) Word_new(STR, strlen(STR))

Word Word_new(const char *str, size_t len);
Word *Word_copy_of(const Word *src, Bump *pool);

bool Word_eq(const Word *a, const Word *b);
bool Word_eq_view(const Word *a, const View *b);

// ordered Word -> id hashmap ==================================================

/*
 * TODO replace with a (much) faster flat version at some point. pretty sure
 * IdMap_remove isn't even used, soooo
 */

typedef struct IdMapNode {
    struct IdMapNode *next;

    const Word *name;
    unsigned id;
} IdMapNode;

typedef struct IdMap {
    IdMapNode **nodes;
    size_t size, cap;
} IdMap;

IdMap IdMap_new(void);
void IdMap_del(IdMap *);

// put does NOT copy words, assumes their lifetime is longer than IdMap
void IdMap_put(IdMap *, const Word *name, unsigned id);

// get will panic if name isn't found, checked allows you to handle errors
bool IdMap_get_checked(const IdMap *, const Word *name, unsigned *out_id);
unsigned IdMap_get(const IdMap *, const Word *name);

void IdMap_remove(IdMap *, const Word *name);

void IdMap_dump(IdMap *);

// put-only id set =============================================================

typedef struct IdSet {
    Vec supersets; // Vec<IdSet *>
    unsigned *ids;
    bool *filled;
    size_t size, cap;
} IdSet;

IdSet IdSet_new(void);
void IdSet_del(IdSet *);

void IdSet_add_superset(IdSet *, IdSet *super);
void IdSet_put(IdSet *, unsigned id);
bool IdSet_has(const IdSet *, unsigned id);

// put-only hash map + set =====================================================

typedef struct HashMap {
    Word *keys;
    void **values;
    size_t size, cap;

    unsigned is_set: 1;
} HashMap;

typedef struct HashSet { HashMap map; } HashSet;

HashMap HashMap_new(void);
void HashMap_del(HashMap *);

// key strings are NOT copied, assumed to be owned by HashMap owner
void HashMap_put(HashMap *, const Word *key, void *value);

// HashMap_get panics, checked does not
bool HashMap_get_checked(const HashMap *, const Word *key, void **o_value);
void *HashMap_get(const HashMap *, const Word *key);
// matches as much of the key as possible, returns length matched
size_t HashMap_get_longest(const HashMap *, const View *key, void **o_value);

HashSet HashSet_new(void);
void HashSet_del(HashSet *);

void HashSet_put(HashSet *, const Word *word);
bool HashSet_has(const HashSet *, const Word *word);
// matches as many chars as possible, returning length
size_t HashSet_longest(const HashSet *, const View *word);

void HashSet_print(const HashSet *);

#endif
