#include <stdlib.h>
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

// ensures a slot is available, returns pointer to that slot
void **Vec_alloc(Vec *v) {
    if (v->len + 1 >= v->cap) {
        v->cap *= 2;
        v->data = realloc(v->data, v->cap * sizeof(*v->data));
    }

  return &v->data[v->len++];
}

void Vec_push(Vec *v, const void *item) {
    *Vec_alloc(v) = (void *)item;
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
