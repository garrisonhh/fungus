#include <assert.h>
#include <ctype.h>

#include "types.h"
#include "names.h"

typedef struct TypeEntry {
    const Word *name;

    // Type ids for the types that this type implements
    IdSet impls;
} TypeEntry;

Bump type_pool;
Vec type_entries; // Vec<Word *>

void types_init(void) {
    type_pool = Bump_new();
    type_entries = Vec_new();
}

void types_quit(void) {
    for (size_t i = 0; i < type_entries.len; ++i)
        IdSet_del(&((TypeEntry *)type_entries.data[i])->impls);

    Vec_del(&type_entries);
    Bump_del(&type_pool);
}

void types_dump(void) {
    puts(TC_CYAN "Types:" TC_RESET);

    for (size_t i = 0; i < type_entries.len; ++i) {
        const TypeEntry *entry = type_entries.data[i];

        printf("%.*s", (int)entry->name->len, entry->name->str);

        if (entry->impls.size > 0) {
            printf(" ::");

            for (size_t j = 0; j < type_entries.len; ++j) {
                if (j == i)
                    continue;

                if (IdSet_has(&entry->impls, j)) {
                    const Word *name = Type_name((Type){ j });

                    printf(" %.*s", (int)name->len, name->str);
                }
            }
        }

        puts("");
    }

    puts("");
}

static TypeEntry *Type_get(Type ty) {
    return type_entries.data[ty.id];
}

const Word *Type_name(Type ty) {
    return Type_get(ty)->name;
}

Type Type_define(Names *names, Word name, Type *supers, size_t num_supers) {
    Type handle = { type_entries.len };
    TypeEntry *entry = Bump_alloc(&type_pool, sizeof(*entry));

    *entry = (TypeEntry){
        .name = Word_copy_of(&name, &type_pool),
        .impls = IdSet_new()
    };

    for (size_t i = 0; i < num_supers; ++i) {
        IdSet_add_superset(&Type_get(supers[i])->impls, &entry->impls);
        IdSet_put(&entry->impls, supers[i].id);
    }

    Vec_push(&type_entries, entry);
    Names_define_type(names, &name, TypeExpr_atom(&names->pool, handle));

    return handle;
}

TypeExpr *TypeExpr_deepcopy(Bump *pool, const TypeExpr *expr) {
    TypeExpr *copy = Bump_alloc(pool, sizeof(*copy));

    copy->type = expr->type;

    switch (expr->type) {
    case TET_ATOM:
        copy->atom = expr->atom;
        break;
    case TET_SUM:
        copy->len = expr->len;
        copy->exprs = Bump_alloc(pool, copy->len * sizeof(*copy->exprs));

        for (size_t i = 0; i < expr->len; ++i)
            copy->exprs[i] = TypeExpr_deepcopy(pool, expr->exprs[i]);

        break;
    }

    return copy;
}

bool TypeExpr_deepequals(const TypeExpr *expr, const TypeExpr *other) {
    if (expr->type != other->type)
        return false;

    switch (expr->type) {
    case TET_ATOM:
        return expr->atom.id == other->atom.id;
    case TET_SUM:
        if (expr->len != other->len)
            return false;

        fungus_panic("TODO sum deepequals");

        return true;
    }
}

bool Type_is(Type ty, Type of) {
    // either types are equivalent, or `ty` subtypes `of`
    return ty.id == of.id || IdSet_has(&Type_get(ty)->impls, of.id);
}

bool TypeExpr_is(const TypeExpr *expr, Type of) {
    return TypeExpr_matches(expr, &(TypeExpr){
        .type = TET_ATOM,
        .atom = of
    });
}

bool Type_matches(Type ty, const TypeExpr *pat) {
    switch (pat->type) {
    case TET_ATOM:;
        // whether atom is the other atom
        return Type_is(ty, pat->atom);
    case TET_SUM:
        // whether atom is included in sum
        for (size_t i = 0; i < pat->len; ++i)
            if (Type_matches(ty, pat->exprs[i]))
                return true;

        return false;
    }
}

bool TypeExpr_matches(const TypeExpr *expr, const TypeExpr *pat) {
    switch (expr->type) {
    case TET_ATOM:
        return Type_matches(expr->atom, pat);
    case TET_SUM:
        // whether expr is a subset of pat
        for (size_t i = 0; i < expr->len; ++i)
            if (!TypeExpr_matches(expr->exprs[i], pat))
                return false;

        return true;
    }
}

bool TypeExpr_equals(const TypeExpr *a, const TypeExpr *b) {
    if (a->type != b->type)
        return false;

    switch (a->type) {
    case TET_ATOM:
        return a->atom.id == b->atom.id;
    case TET_SUM:
        if (a->len != b->len)
            return false;

        // TODO this is slow as balls. implement a TypeSet for sums
        for (size_t i = 0; i < a->len; ++i) {
            bool found = false;

            for (size_t j = 0; j < b->len; ++j) {
                if (TypeExpr_equals(a->exprs[i], b->exprs[j])) {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;
        }

        return true;
    }
}

TypeExpr *TypeExpr_atom(Bump *pool, Type ty) {
    TypeExpr *te = Bump_alloc(pool, sizeof(*te));

    te->type = TET_ATOM;
    te->atom = ty;

    return te;
}

static TypeExpr *TE_copy_va_list(Bump *pool, size_t n, va_list argp) {
    TypeExpr *te = Bump_alloc(pool, sizeof(*te));

    te->len = n;
    te->exprs = Bump_alloc(pool, te->len * sizeof(*te->exprs));

    for (size_t i = 0; i < te->len; ++i)
        te->exprs[i] = va_arg(argp, TypeExpr *);

    return te;
}

TypeExpr *TypeExpr_sum(Bump *pool, size_t n, ...) {
    va_list argp;

    va_start(argp, n);
    TypeExpr *te = TE_copy_va_list(pool, n, argp);
    va_end(argp);

    te->type = TET_SUM;

    return te;
}

TypeExpr *TypeExpr_product(Bump *pool, size_t n, ...) {
    va_list argp;

    va_start(argp, n);
    TypeExpr *te = TE_copy_va_list(pool, n, argp);
    va_end(argp);

    te->type = TET_SUM;

    return te;
}

void TypeExpr_print(const TypeExpr *expr) {
    switch (expr->type) {
    case TET_ATOM:
        printf(TC_BLUE);
        Type_print(expr->atom);
        printf(TC_RESET);
        break;
    case TET_SUM:
        for (size_t i = 0; i < expr->len; ++i) {
            if (i) printf(" | ");
            TypeExpr_print(expr->exprs[i]);
        }
        break;
    }
}

void Type_print(Type ty) {
    const Word *name = Type_name(ty);

    bool symbolic = ispunct(name->str[0]);

    if (symbolic) printf("`");
    printf("%.*s", (int)name->len, name->str);
    if (symbolic) printf("`");
}
