#include "precedence.h"

PrecGraph PrecGraph_new(void) {
    return (PrecGraph){
        .pool = Bump_new(),
        .entries = Vec_new(),
        .by_name = IdMap_new()
    };
}

void PrecGraph_del(PrecGraph *pg) {
    IdMap_del(&pg->by_name);
    Vec_del(&pg->entries);
    Bump_del(&pg->pool);
}

static void *PG_alloc(PrecGraph *pg, size_t n_bytes) {
    return Bump_alloc(&pg->pool, n_bytes);
}

static PrecEntry *PG_get(const PrecGraph *pg, Prec handle) {
    return pg->entries.data[handle.id];
}

static bool PG_higher_than(const PrecGraph *pg, PrecEntry *entry, Prec object) {
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
    // create entry from def
    PrecEntry *entry = PG_alloc(pg, sizeof(*entry));
    Prec handle = (Prec){ pg->entries.len };

    *entry = (PrecEntry){
        .name = Word_copy_of(&def->name, &pg->pool),
        .above_len = def->above_len
    };

    for (size_t i = 0; i < def->above_len; ++i)
        entry->above[i] = def->above[i];

    Vec_push(&pg->entries, entry);
    IdMap_put(&pg->by_name, entry->name, handle.id);

    // add to any below lists
    for (size_t i = 0; i < def->below_len; ++i) {
        PrecEntry *below_entry = PG_get(pg, def->below[i]);

        // verify this isn't idiotic TODO this should be an error not a panic
        if (PG_higher_than(pg, entry, def->below[i])) {
            fungus_panic("attempted circular precedence definition: "
                         "%.*s <=> %.*s",
                         (int)entry->name->len, entry->name->str,
                         (int)below_entry->name->len, below_entry->name->str);
        }

        below_entry->above[below_entry->above_len++] = handle;
    }

    return handle;
}

bool Prec_by_name_checked(PrecGraph *pg, const Word *name, Prec *o_prec) {
    unsigned id;

    if (IdMap_get_checked(&pg->by_name, name, &id)) {
        o_prec->id = id;

        return true;
    }

    return false;
}

Prec Prec_by_name(PrecGraph *pg, const Word *name) {
    Prec res;

    if (Prec_by_name_checked(pg, name, &res))
        return res;

    fungus_panic("failed to retrieve precedence %.*s.",
                 (int)name->len, name->str);
}

Comparison Prec_cmp(const PrecGraph *pg, Prec a, Prec b) {
    if (PG_higher_than(pg, PG_get(pg, a), b))
        return GT;
    else if (PG_higher_than(pg, PG_get(pg, b), a))
        return LT;
    else
        return EQ;
}

void PrecGraph_dump(const PrecGraph *pg) {
    puts(TC_CYAN "PrecGraph:" TC_RESET);

    for (size_t i = 0; i < pg->entries.len; ++i) {
        PrecEntry *entry = pg->entries.data[i];

        printf("%.*s <%s>", (int)entry->name->len, entry->name->str,
               entry->assoc == ASSOC_LEFT ? "left" : "right");

        if (entry->above_len) {
            printf(" >");

            for (size_t j = 0; j < entry->above_len; ++j) {
                const Word *above_name = PG_get(pg, entry->above[j])->name;

                printf(" %.*s", (int)above_name->len, above_name->str);
            }
        }

        puts("");
    }

    puts("");
}
