#include "precedence.h"

PrecGraph PrecGraph_new(void) {
    return (PrecGraph){
        .pool = Bump_new(),
        .entries = Vec_new()
    };
}

void PrecGraph_del(PrecGraph *pg) {
    Vec_del(&pg->entries);
    Bump_del(&pg->pool);
}

static void *PG_alloc(PrecGraph *pg, size_t n_bytes) {
    return Bump_alloc(&pg->pool, n_bytes);
}

static PrecEntry *PG_get(PrecGraph *pg, Prec handle) {
    if (handle.id > pg->entries.len)
        fungus_panic("attempted to get an invalid precedence id.");

    return pg->entries.data[handle.id];
}

static bool PG_higher_than(PrecGraph *pg, PrecEntry *entry, Prec object) {
    // check if object is directly contained in lhs->above
    for (size_t i = 0; i < entry->above_len; ++i)
        if (entry->above[i].id == object.id)
            return true;

    // check if object is indirectly contained in lhs->above
    for (size_t i = 0; i < entry->above_len; ++i)
        if (PG_higher_than(pg, PG_get(pg, entry->above[i]), object))
            return true;

    return false;
}

Prec Prec_define(PrecGraph *pg, PrecDef *def) {
    // create entry
    PrecEntry *entry = PG_alloc(pg, sizeof(*entry));
    Prec handle = (Prec){ pg->entries.len };

    *entry = (PrecEntry){
        .name = Word_copy_of(&def->name, &pg->pool),
        .above_len = def->above_len
    };

    Vec_push(&pg->entries, entry);

    // copy above list
    memcpy(entry->above, def->above, def->above_len * sizeof(*def->above));

    // add to below lists
    for (size_t i = 0; i < def->below_len; ++i) {
        PrecEntry *below_entry = PG_get(pg, def->below[i]);

        // verify this isn't idiotic TODO this should be an error not a panic
        if (PG_higher_than(pg, entry, def->below[i])) {
            fungus_panic("attempted circular precedence definition: "
                         "%.*s <-> %.*s",
                         (int)entry->name->len, entry->name->str,
                         (int)below_entry->name->len, below_entry->name->str);
        }

        below_entry->above[below_entry->above_len++] = handle;
    }

    return handle;
}

Comparison Prec_cmp(PrecGraph *pg, Prec a, Prec b) {
    if (PG_higher_than(pg, PG_get(pg, a), b))
        return GT;
    else if (PG_higher_than(pg, PG_get(pg, b), a))
        return LT;
    else
        return EQ;
}

void PrecGraph_dump(PrecGraph *pg) {
    for (size_t i = 0; i < pg->entries.len; ++i) {
        PrecEntry *entry = pg->entries.data[i];

        printf("  %.*s\n", (int)entry->name->len, entry->name->str);

        for (size_t j = 0; j < entry->above_len; ++j) {
            Word *above_name = PG_get(pg, entry->above[j])->name;

            printf("    %.*s\n", (int)above_name->len, above_name->str);
        }

        puts("");
    }
}
