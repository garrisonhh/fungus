#include <string.h>

#include "words.h"

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

bool Word_eq(Word *a, Word *b) {
    // NOTE if this is broken, fnv-1a may have to go
    return a->len == b->len && a->hash == b->hash;
}

bool Word_eq_view(Word *a, View *b) {
    Word bw = Word_new(b->str, b->len);

    return Word_eq(a, &bw);
}
