#include <assert.h>
#include <stdalign.h>

#include "data.h"

#ifndef DATA_INIT_CAP
#define DATA_INIT_CAP 8
#endif

// dyn array ===================================================================

Vec Vec_new(void) {
    Vec v = {
        .data = malloc(DATA_INIT_CAP * sizeof(*v.data)),
        .cap = DATA_INIT_CAP
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
    if (--v->len < v->cap / 2) {
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
    v->cap = DATA_INIT_CAP;
    v->data = realloc(v->data, v->cap * sizeof(*v->data));
}

void Vec_push(Vec *v, const void *item) {
    *(const void **)Vec_alloc(v) = item;
}

void *Vec_pop(Vec *v) {
    void *item = v->data[v->len - 1];

    Vec_smolify(v);

    return item;
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

#ifdef DEBUG
    b->allocated += BUMP_PAGE_SIZE;
#endif
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
    DEBUG_SCOPE(0,
        char buf[256];
        sprintf(buf, "%zu/%zu", b->total, b->allocated);
        printf("deleting bump: %10s\n", buf);
    );

    for (size_t i = 0; i < b->pages.len; ++i)
        free(b->pages.data[i]);

    for (size_t i = 0; i < b->lost_n_found.len; ++i)
        free(b->lost_n_found.data[i]);

    Vec_del(&b->pages);
    Vec_del(&b->lost_n_found);
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

#ifdef DEBUG
        b->allocated += nbytes;
#endif

        return mem;
    } else if (nbytes + b->bump > BUMP_PAGE_SIZE) {
        // page would overflow if allocating this memory, need a new one
        new_page(b);
    }

    // allocate from current page
    void *ptr = &b->page[b->bump];

    b->bump += nbytes;

#ifdef DEBUG
    b->total += nbytes;
#endif

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

#ifdef DEBUG
    b->total = 0;
#endif
}

// views + words ===============================================================

bool View_eq(const View *a, const View *b) {\
    return a->len == b->len && !strncmp(a->str, b->str, a->len);
}

char View_get(const View *v, size_t index) {
    return index < v->len ? v->str[index] : '\0';
}

Word Word_new(const char *str, size_t len) {
    return (Word){
        .str = str,
        .len = len,
        .hash = fnv_hash(str, len),
    };
}

Word *Word_copy_of(const Word *src, Bump *pool) {
    char *str = Bump_alloc(pool, src->len * sizeof(*str));
    Word *copy = Bump_alloc(pool, sizeof(*copy));

    strncpy(str, src->str, src->len);
    *copy = Word_new(str, src->len);

    return copy;
}

bool Word_eq(const Word *a, const Word *b) {
    return a->len == b->len && a->hash == b->hash
        && !strncmp(a->str, b->str, a->len);
}

bool Word_eq_view(const Word *a, const View *b) {
    return a->len == b->len && !strncmp(a->str, b->str, a->len);
}

// idmap =======================================================================

IdMap IdMap_new(void) {
    return (IdMap){
        .nodes = calloc(DATA_INIT_CAP, sizeof(IdMapNode *)),
        .cap = DATA_INIT_CAP
    };
}

void IdMap_del(IdMap *map) {
    for (size_t i = 0; i < map->cap; ++i) {
        IdMapNode *trav = map->nodes[i];

        while (trav) {
            IdMapNode *next = trav->next;

            free(trav);
            trav = next;
        }
    }

    free(map->nodes);
}

static void IdMap_put_lower(IdMap *map, IdMapNode *node) {
    IdMapNode **place = &map->nodes[node->name->hash % map->cap];

    node->next = *place;
    *place = node;
}

static void IdMap_resize(IdMap *map, size_t new_cap) {
    IdMapNode **old_nodes = map->nodes;
    size_t old_cap = map->cap;

    map->nodes = calloc(new_cap, sizeof(*map->nodes));
    map->cap = new_cap;

    for (size_t i = 0; i < old_cap; ++i) {
        IdMapNode *trav = old_nodes[i];

        while (trav) {
            IdMapNode *next = trav->next;

            IdMap_put_lower(map, trav);
            trav = next;
        }
    }

    free(old_nodes);
}

void IdMap_put(IdMap *map, const Word *name, unsigned id) {
    // check for expansion
    if (map->size * 2 > map->cap)
        IdMap_resize(map, map->cap * 2);

    // place node
    // TODO fml the memory fragmentation :(
    IdMapNode *node = malloc(sizeof(*node));

    *node = (IdMapNode){ .name = name, .id = id };

    IdMap_put_lower(map, node);

    ++map->size;
}

bool IdMap_get_checked(const IdMap *map, const Word *name, unsigned *out_id) {
    IdMapNode *trav = map->nodes[name->hash % map->cap];

    for (; trav; trav = trav->next) {
        if (Word_eq(name, trav->name)) {
            *out_id = trav->id;

            return true;
        }
    }

    return false;
}

unsigned IdMap_get(const IdMap *map, const Word *name) {
    unsigned id;

    if (IdMap_get_checked(map, name, &id))
        return id;

    fungus_panic("IdMap failed to retrieve '%.*s'!\n",
                 (int)name->len, name->str);
}

void IdMap_remove(IdMap *map, const Word *name) {
    // TODO if this function remains unused that would be REALLY nice for
    // a replacement implementation
    fungus_panic("IDMap_remove not impl'd");
}

void IdMap_dump(IdMap *map) {
    puts("IdMap:");

    for (size_t i = 0; i < map->cap; ++i) {
        if (map->nodes[i]) {
            printf("%4zu | ", i);

            for (IdMapNode *trav = map->nodes[i]; trav; trav = trav->next) {
                printf("'%.*s'", (int)trav->name->len, trav->name->str);
                if (trav->next) printf(" -> ");
            }

            puts("");
        }
    }
}

// put-only id set =============================================================

IdSet IdSet_new(void) {
    return (IdSet){
        .supersets = Vec_new(),
        .ids = malloc(DATA_INIT_CAP * sizeof(unsigned)),
        .filled = calloc(DATA_INIT_CAP, sizeof(bool)),
        .cap = DATA_INIT_CAP
    };
}

void IdSet_del(IdSet *set) {
    Vec_del(&set->supersets);
    free(set->ids);
    free(set->filled);
}

static void IdSet_put_lower(IdSet *set, unsigned id) {
    size_t idx = fnv_hash((const char *)&id, sizeof(id)) % set->cap;

    while (set->filled[idx])
        idx = (idx + 1) % set->cap;

    set->ids[idx] = id;
    set->filled[idx] = true;
}

static void IdSet_resize(IdSet *set, size_t new_cap) {
    unsigned *old_ids = set->ids;
    bool *old_filled = set->filled;
    size_t old_cap = set->cap;

    set->cap = new_cap;
    set->ids = malloc(set->cap * sizeof(*set->ids));
    set->filled = calloc(set->cap, sizeof(*set->filled));

    for (size_t i = 0; i < old_cap; ++i)
        if (old_filled[i])
            IdSet_put_lower(set, old_ids[i]);
}

void IdSet_add_superset(IdSet *set, IdSet *super) {
    assert(set != super);

    Vec_push(&set->supersets, super);

    for (size_t i = 0; i < set->cap; ++i)
        if (set->filled[i])
            IdSet_put(super, set->ids[i]);
}

void IdSet_put(IdSet *set, unsigned id) {
    // check resize
    if (set->size * 2 > set->cap)
        IdSet_resize(set, set->cap * 2);

    // add to this set
    IdSet_put_lower(set, id);

    ++set->size;

    // add to supersets
    for (size_t i = 0; i < set->supersets.len; ++i)
        IdSet_put(set->supersets.data[i], id);
}

bool IdSet_has(const IdSet *set, unsigned id) {
    size_t idx = fnv_hash((const char *)&id, sizeof(id)) % set->cap;

    while (set->filled[idx]) {
        if (set->ids[idx] == id)
            return true;

        idx = (idx + 1) % set->cap;
    }

    return false;
}

// put-only pash map + set =====================================================

static HashMap HashMap_new_lower(bool is_set) {
    return (HashMap){
        .keys = calloc(DATA_INIT_CAP, sizeof(Word)),
        .values = is_set ? NULL : malloc(DATA_INIT_CAP * sizeof(void *)),
        .cap = DATA_INIT_CAP,

        .is_set = is_set
    };
}

HashMap HashMap_new(void) {
    return HashMap_new_lower(false);
}

void HashMap_del(HashMap *map) {
    free(map->keys);
    free(map->values);
}

static void HashMap_put_lower(HashMap *map, const Word *key, void *value) {
    size_t index = key->hash % map->cap;

    while (map->keys[index].str)
        index = (index + 1) % map->cap;

    map->keys[index] = *key;

    if (!map->is_set)
        map->values[index] = value;
}

static void HashMap_resize(HashMap *map, size_t new_cap) {
    size_t old_cap = map->cap;
    Word *old_keys = map->keys;
    void **old_values = map->values;

    map->cap = new_cap;
    map->keys = calloc(map->cap, sizeof(*map->keys));

    if (!map->is_set)
        map->values = calloc(map->cap, sizeof(*map->values));

    for (size_t i = 0; i < old_cap; ++i) {
        if (old_keys[i].str) {
            HashMap_put_lower(map, &old_keys[i],
                              map->is_set ? NULL : old_values[i]);
        }
    }

    free(old_keys);
    free(old_values);
}

void HashMap_put(HashMap *map, const Word *key, void *value) {
    if (map->size * 2 > map->cap)
        HashMap_resize(map, map->cap * 2);

    HashMap_put_lower(map, key, value);

    ++map->size;
}

bool HashMap_get_checked(const HashMap *map, const Word *key, void **o_value) {
    size_t index = key->hash % map->cap;

    while (map->keys[index].str) {
        if (Word_eq(&map->keys[index], key)) {
            if (!map->is_set)
                *o_value = map->values[index];

            return true;
        }

        index = (index + 1) % map->cap;
    }

    return false;
}

void *HashMap_get(const HashMap *map, const Word *key) {
    void *value;

    if (HashMap_get_checked(map, key, &value))
        return value;

    fungus_panic("HashMap failed to retrieve '%.*s'!\n",
                 (int)key->len, key->str);
}

size_t HashMap_get_longest(const HashMap *map, const View *key, void **o_val) {
    hash_t hash = fnv_hash_start();
    size_t matched = 0;
    void *best = NULL;

    for (size_t i = 0; i < key->len; ++i) {
        hash = fnv_hash_next(hash, key->str[i]);

        Word slice = {
            .str = key->str,
            .len = i + 1,
            .hash = hash
        };

#ifdef DEBUG
        Word test = Word_new(slice.str, slice.len);

        assert(Word_eq(&slice, &test));
#endif

        void *value = NULL;

        if (HashMap_get_checked(map, &slice, &value)) {
            best = value;
            matched = slice.len;
        }
    }

    if (o_val)
        *o_val = best;

    return matched;
}

HashSet HashSet_new(void) {
    return (HashSet){ HashMap_new_lower(true) };
}

void HashSet_del(HashSet *set) {
    HashMap_del(&set->map);
}

void HashSet_put(HashSet *set, const Word *word) {
    HashMap_put(&set->map, word, NULL);
}

bool HashSet_has(const HashSet *set, const Word *word) {
    return HashMap_get_checked(&set->map, word, NULL);
}

size_t HashSet_longest(const HashSet *set, const View *word) {
    return HashMap_get_longest(&set->map, word, NULL);
}

void HashSet_print(const HashSet *set) {
    bool first = true;

    for (size_t i = 0; i < set->map.cap; ++i) {
        const Word *key = &set->map.keys[i];

        if (key->str) {
            if (first)
                first = false;
            else
                printf(" ");

            printf("%.*s", (int)key->len, key->str);
        }
    }
}
