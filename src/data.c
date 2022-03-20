#include <stdlib.h>
#include <stdio.h>
#include <stdalign.h>

#include "data.h"

// dyn array ===================================================================

#ifndef VEC_INIT_CAP
#define VEC_INIT_CAP 8
#endif

Vec Vec_new(void) {
    Vec v = {
        .data = malloc(VEC_INIT_CAP * sizeof(*v.data)),
        .cap = VEC_INIT_CAP
    };

    return v;
}

void Vec_del(Vec *v) {
    free(v->data);
}

// add one slot
static void Vec_biggify(Vec *v) {
    if (++v->len >= v->cap) {
        v->cap *= 2;
        v->data = realloc(v->data, v->cap * sizeof(*v->data));
    }
}

// subtract one slot
static void Vec_smolify(Vec *v) {
    if (--v->len <= v->cap / 2) {
        v->cap /= 2;
        v->data = realloc(v->data, v->cap * sizeof(*v->data));
    }
}

// ensures a slot is available, returns pointer to that slot
void **Vec_alloc(Vec *v) {
    Vec_biggify(v);

    return &v->data[v->len - 1];
}

void Vec_clear(Vec *v) {
    v->len = 0;
    v->cap = VEC_INIT_CAP;
    v->data = realloc(v->data, v->cap * sizeof(*v->data));
}

void Vec_push(Vec *v, const void *item) {
    *(const void **)Vec_alloc(v) = item;
}

// inserts idx into a slot in O(n) time to maintain sorted order
void Vec_ordered_insert(Vec *v, size_t idx, const void *item) {
    if (idx > v->len) {
        fprintf(stderr, "attempted to insert item in Vec past end of array.\n");
        exit(-1);
    }

    Vec_alloc(v);

    for (size_t i = v->len - 1; i > idx; --i)
        v->data[i] = v->data[i - 1];

    v->data[idx] = (void *)item;
}

void Vec_qsort(Vec *v, int (*cmp)(const void *, const void *)) {
    qsort(v->data, v->len, sizeof(*v->data), cmp);
}

// bump memory =================================================================

#ifndef BUMP_PAGE_SIZE
#define BUMP_PAGE_SIZE 4096
#endif

#define ALIGNMENT (alignof(max_align_t))

static void new_page(Bump *b) {
    b->page = malloc(BUMP_PAGE_SIZE);
    b->bump = 0;

    Vec_push(&b->pages, b->page);
}

Bump Bump_new(void) {
    Bump b = {
        .pages = Vec_new(),
        .lost_n_found = Vec_new()
    };

    new_page(&b);

    return b;
}

void Bump_del(Bump *b) {
    for (size_t i = 0; i < b->pages.len; ++i)
        free(b->pages.data[i]);

    for (size_t i = 0; i < b->lost_n_found.len; ++i)
        free(b->lost_n_found.data[i]);
}

void *Bump_alloc(Bump *b, size_t nbytes) {
    // align nbytes
    size_t align_n = nbytes % ALIGNMENT;

    if (align_n)
        nbytes += ALIGNMENT - align_n;

    if (nbytes > BUMP_PAGE_SIZE) {
        // custom large page
        char *mem = malloc(nbytes);

        Vec_push(&b->lost_n_found, mem);

        return mem;
    } else if (nbytes + b->bump > BUMP_PAGE_SIZE) {
        // page would overflow if allocating this memory, need a new one
        new_page(b);
    }

    // allocate from current page
    void *ptr = &b->page[b->bump];

    b->bump += nbytes;

    return ptr;
}

void Bump_clear(Bump *b) {
    // free all pages but the first
    for (size_t i = 1; i < b->pages.len; ++i)
        free(b->pages.data[i]);

    for (size_t i = 0; i < b->lost_n_found.len; ++i)
        free(b->lost_n_found.data[i]);

    // reset bump
    b->page = b->pages.data[0];
    b->bump = 0;
}

// views + words ===============================================================

Word Word_new(const char *str, size_t len) {
    return (Word){
        .str = str,
        .len = len,
        .hash = fnv_hash(str, len),
    };
}

Word *Word_copy_of(Word *src, Bump *pool) {
    char *str = Bump_alloc(pool, src->len * sizeof(*str));
    Word *copy = Bump_alloc(pool, sizeof(*copy));

    strncpy(str, src->str, src->len);
    *copy = Word_new(str, src->len);

    return copy;
}

bool Word_eq(const Word *a, const Word *b) {
    // NOTE if this is broken, fnv-1a may have to go
    return a->len == b->len && a->hash == b->hash;
}

bool Word_eq_view(const Word *a, const View *b) {
    Word bw = Word_new(b->str, b->len);

    return Word_eq(a, &bw);
}

// idmap =======================================================================

