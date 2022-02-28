#ifndef DATA_H
#define DATA_H

#include <stddef.h>

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

#endif
