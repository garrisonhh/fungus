#include <assert.h>
#include <ctype.h>

#include "types.h"

/*
 * TypeHandle uses the top 2 bytes to store a TypeGraph id, and the bottom
 * 2 to store a Type id local to its TypeGraph
 */
static_assert(sizeof(((Type *)0)->id) == sizeof(unsigned),
              "Type handle bits are broken :(");

TypeGraph TypeGraph_new(void) {
    static uint16_t id_counter = 0;

    TypeGraph tg = {
        .pool = Bump_new(),
        .entries = Vec_new(),
        .by_name = IdMap_new(),
        .id = id_counter++
    };

    // Any type
    Type_define(&tg, &(TypeDef){
        .name = WORD("Any"),
        .type = TTY_ABSTRACT
    });

    return tg;
}

void TypeGraph_del(TypeGraph *tg) {
    for (size_t i = 0; i < tg->entries.len; ++i) {
        TypeEntry *entry = tg->entries.data[i];

        if (entry->type == TTY_ABSTRACT)
            IdSet_del(entry->type_set);
    }

    IdMap_del(&tg->by_name);
    Vec_del(&tg->entries);
    Bump_del(&tg->pool);
}

static void *TG_alloc(TypeGraph *tg, size_t n_bytes) {
    return Bump_alloc(&tg->pool, n_bytes);
}

bool Type_is_in(const TypeGraph *tg, Type ty) {
    return (ty.id >> 16) == tg->id;
}

TypeEntry *Type_get(const TypeGraph *tg, Type handle) {
    if (!Type_is_in(tg, handle))
        fungus_panic("Wrong TypeGraph for handle :(");

    return tg->entries.data[handle.id & 0xFFFF];
}

static bool validate_def(TypeGraph *tg, TypeDef *def) {
    // TODO verify def->expr

    bool abstracts_are_abstract = true;

    for (size_t i = 0; i < def->is_len; ++i) {
        if (Type_get(tg, def->is[i])->type != TTY_ABSTRACT) {
            abstracts_are_abstract = false;
            break;
        }
    }

    return abstracts_are_abstract;
}

TypeExpr *TypeExpr_deepcopy(Bump *pool, TypeExpr *expr) {
    TypeExpr *copy = Bump_alloc(pool, sizeof(*copy));

    copy->type = expr->type;

    switch (expr->type) {
    case TET_ATOM:
        copy->atom = expr->atom;
        break;
    case TET_SUM:
    case TET_PRODUCT:
        copy->len = expr->len;
        copy->exprs = Bump_alloc(pool, copy->len * sizeof(*copy->exprs));

        for (size_t i = 0; i < expr->len; ++i)
            copy->exprs[i] = TypeExpr_deepcopy(pool, expr->exprs[i]);

        break;
    }

    return copy;
}

/*
static TypeExpr *TG_deepcopy_expr(TypeGraph *tg, TypeExpr *expr) {
    return TypeExpr_deepcopy(&tg->pool, expr);
}
*/

static Type make_handle(TypeGraph *tg, uint16_t local_id) {
    return (Type){ (((uint32_t)tg->id) << 16) | (uint32_t)local_id };
}

static void add_type_to_abstract(TypeGraph *tg, Type ty, TypeEntry *entry,
                                 Type to) {
    TypeEntry *set_entry = Type_get(tg, to);

    IdSet_put(set_entry->type_set, ty.id);

    if (entry->type == TTY_ABSTRACT)
        IdSet_add_superset(entry->type_set, set_entry->type_set);
}

Type Type_define(TypeGraph *tg, TypeDef *def) {
    // validate
    if (!validate_def(tg, def)) {
        fungus_panic("invalid type definition: %.*s",
                     (int)def->name.len, def->name.str);
    }

    // create entry
    TypeEntry *entry = TG_alloc(tg, sizeof(*entry));
    Type handle = make_handle(tg, tg->entries.len);

    *entry = (TypeEntry){
        .name = Word_copy_of(&def->name, &tg->pool),
        .type = def->type
    };

    if (def->type == TTY_ABSTRACT) {
        entry->type_set = TG_alloc(tg, sizeof(*entry->type_set));
        *entry->type_set = IdSet_new();
    }

    // place id in abstract type sets
    for (size_t i = 0; i < def->is_len; ++i)
        add_type_to_abstract(tg, handle, entry, def->is[i]);

    // place id in Any set
    if (tg->entries.len > 0)
        add_type_to_abstract(tg, handle, entry, tg->ty_any);

    Vec_push(&tg->entries, entry);
    IdMap_put(&tg->by_name, entry->name, handle.id);

    return handle;
}

bool Type_by_name(const TypeGraph *tg, Word *name, Type *o_type) {
    return IdMap_get_checked(&tg->by_name, name, &o_type->id);
}

const Word *Type_name(const TypeGraph *tg, Type ty) {
    return Type_get(tg, ty)->name;
}

bool TypeExpr_deepequals(TypeExpr *expr, TypeExpr *other) {
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
    case TET_PRODUCT:
        if (expr->len != other->len)
            return false;

        for (size_t i = 0; i < expr->len; ++i)
            if (!TypeExpr_deepequals(expr->exprs[i], other->exprs[i]))
                return false;

        return true;
    }
}

bool Type_is(const TypeGraph *tg, Type ty, Type of) {
    // check self
    if (ty.id == of.id)
        return true;

    TypeEntry *of_entry = Type_get(tg, of);

    return of_entry->type == TTY_ABSTRACT
        && IdSet_has(of_entry->type_set, ty.id);
}

bool TypeExpr_is(const TypeGraph *tg, const TypeExpr *expr, Type of) {
    return TypeExpr_matches(tg, expr, &(TypeExpr){
        .type = TET_ATOM,
        .atom = of
    });
}

bool Type_matches(const TypeGraph *tg, Type ty, const TypeExpr *pat) {
    switch (pat->type) {
    case TET_ATOM:;
        // whether atom is the other atom
        return Type_is(tg, ty, pat->atom);
    case TET_PRODUCT:
        // atoms can't match products
        return false;
    case TET_SUM:
        // whether atom is included in sum
        for (size_t i = 0; i < pat->len; ++i)
            if (Type_matches(tg, ty, pat->exprs[i]))
                return true;

        return false;
    }
}

bool TypeExpr_matches(const TypeGraph *tg, const TypeExpr *expr,
                      const TypeExpr *pat) {
    switch (expr->type) {
    case TET_ATOM:
        return Type_matches(tg, expr->atom, pat);
    case TET_SUM:
        // whether expr is a subset of pat
        for (size_t i = 0; i < expr->len; ++i)
            if (!TypeExpr_matches(tg, expr->exprs[i], pat))
                return false;

        return true;
    case TET_PRODUCT:
        switch (pat->type) {
        case TET_ATOM:
            // products can't match atoms
            return false;
        case TET_SUM:
            // products could match something in a sum
            for (size_t i = 0; i < pat->len; ++i)
                if (TypeExpr_matches(tg, expr, pat->exprs[i]))
                    return true;

            return false;
        case TET_PRODUCT:
            // whether all of expr members match pat's members
            if (pat->len != expr->len)
                return false;

            for (size_t i = 0; i < expr->len; ++i)
                if (!TypeExpr_matches(tg, expr->exprs[i], pat->exprs[i]))
                    return false;

            return true;
        }
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
    case TET_PRODUCT:
        if (a->len != b->len)
            return false;

        for (size_t i = 0; i < a->len; ++i)
            if (!TypeExpr_equals(a->exprs[i], b->exprs[i]))
                return false;

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

void TypeExpr_print(const TypeGraph *tg, const TypeExpr *expr) {
    switch (expr->type) {
    case TET_ATOM:
        Type_print(tg, expr->atom);
        break;
    case TET_SUM:
        for (size_t i = 0; i < expr->len; ++i) {
            bool add_parens = expr->exprs[i]->type == TET_PRODUCT;

            if (i) printf(" | ");
            if (add_parens) printf("(");
            TypeExpr_print(tg, expr->exprs[i]);
            if (add_parens) printf(")");
        }
        break;
    case TET_PRODUCT:
        for (size_t i = 0; i < expr->len; ++i) {
            if (i) printf(" * ");
            TypeExpr_print(tg, expr->exprs[i]);
        }
        break;
    }
}

void Type_print(const TypeGraph *tg, Type ty) {
    TypeEntry *entry = Type_get(tg, ty);

    bool symbolic = ispunct(entry->name->str[0]);

    if (symbolic) printf("`");
    printf("%.*s", (int)entry->name->len, entry->name->str);
    if (symbolic) printf("`");
}

void Type_print_verbose(const TypeGraph *tg, Type ty) {
    Type_print(tg, ty);

    TypeEntry *entry = Type_get(tg, ty);

    if (entry->type == TTY_ABSTRACT) {
        printf(": {");

        IdSet *type_set = entry->type_set;

        printf(": {");

        for (size_t i = 0; i < type_set->cap; ++i) {
            if (type_set->filled[i]) {
                printf(" ");
                Type_print(tg, (Type){ type_set->ids[i] });
            }
        }

        printf(" }");

    }
}

void TypeGraph_dump(const TypeGraph *tg) {
    puts(TC_CYAN "TypeGraph:" TC_RESET);
    puts("abstract:");

    for (size_t i = 0; i < tg->entries.len; ++i) {
        Type ty = { i };

        if (Type_get(tg, ty)->type != TTY_ABSTRACT)
            continue;

        printf("  ");
        Type_print_verbose(tg, ty);
        puts("");
    }

    puts("concrete:");
    printf("  ");

    for (size_t i = 0; i < tg->entries.len; ++i) {
        Type ty = { i };

        if (Type_get(tg, ty)->type == TTY_ABSTRACT)
            continue;

        Type_print_verbose(tg, ty);
        printf(" ");
    }

    puts("\n");
}
