#include <assert.h>

#include "names.h"

#define TABLE_MIN_CAP 64
#define TABLE_MIN_SCOPES_CAP 16

// word -> NameEntry *
Bump globals_pool;
HashMap globals;

void names_init(void) {
    globals_pool = Bump_new();
    globals = HashMap_new();
}

void names_quit(void) {
    HashMap_del(&globals);
    Bump_del(&globals_pool);
}

Names Names_new(void) {
    return (Names){
        .pool = Bump_new(),

        .cap = TABLE_MIN_CAP,
        .entries = malloc(TABLE_MIN_CAP * sizeof(NameEntry)),

        .scope_cap = TABLE_MIN_SCOPES_CAP,
        .scopes = malloc(TABLE_MIN_SCOPES_CAP * sizeof(size_t)),
    };
}

void Names_del(Names *names) {
    free(names->entries);
    free(names->scopes);
    Bump_del(&names->pool);
}

void Names_push_scope(Names *names) {
    if (names->level + 1 == names->scope_cap) {
        names->scope_cap *= 2;
        names->scopes =
            realloc(names->scopes, names->scope_cap * sizeof(*names->scopes));
    }

    names->scopes[names->level++] = names->len;
}

void Names_drop_scope(Names *names) {
    assert(names->level > 0);

    names->len = names->scopes[--names->level];
}

static void copy_entry_to(Bump *pool, NameEntry *dst, const NameEntry *entry) {
    *dst = (NameEntry){
        .name = Word_copy_of(entry->name, pool),
        .type = entry->type
    };

    switch (entry->type) {
    case NAMED_TYPE:
    case NAMED_VARIABLE:
        dst->type_expr = TypeExpr_deepcopy(pool, entry->type_expr);

        break;
    default:
        UNIMPLEMENTED;
    }
}

// copies an entry into either global or local space
static void put_entry(Names *names, const NameEntry *entry) {
    if (!names || !names->level) {
        // put globally
        NameEntry *copy = Bump_alloc(&globals_pool, sizeof(*copy));
        copy_entry_to(&globals_pool, copy, entry);

        HashMap_put(&globals, entry->name, copy);
    } else {
        // put locally
        if (names->len + 1 == names->cap) {
            names->cap *= 2;
            names->entries =
                realloc(names->entries, names->cap * sizeof(*names->entries));
        }

        NameEntry *store = &names->entries[names->len++];

        copy_entry_to(&names->pool, store, entry);
    }
}

void Names_define_type(Names *names, const Word *name,
                       const TypeExpr *type_expr) {
    put_entry(names, &(NameEntry){
        .name = name,
        .type = NAMED_TYPE,
        .type_expr = type_expr
    });
}

void Names_define_var(Names *names, const Word *name, Type type) {
    put_entry(names, &(NameEntry){
        .name = name,
        .type = NAMED_VARIABLE,
        .var_type = type
    });
}

const NameEntry *name_lookup(const Names *names, const Word *name) {
    // walk up vars to find this one
    for (int i = names->len - 1; i >= 0; --i) {
        const NameEntry *entry = &names->entries[i];

        if (Word_eq(entry->name, name))
            return entry;
    }

    // not found yet, check globals
    void *found;

    if (HashMap_get_checked(&globals, name, &found))
        return found;

    // name doesn't exist
    return NULL;
}
