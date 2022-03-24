#include <ctype.h>

#include "types.h"

TypeGraph TypeGraph_new(void) {
    return (TypeGraph){
        .pool = Bump_new(),
        .entries = Vec_new(),
        .by_name = IdMap_new()
    };
}

void TypeGraph_del(TypeGraph *tg) {
    // TODO abstract type sets are leaked

    IdMap_del(&tg->by_name);
    Vec_del(&tg->entries);
    Bump_del(&tg->pool);
}

static void *TG_alloc(TypeGraph *tg, size_t n_bytes) {
    return Bump_alloc(&tg->pool, n_bytes);
}

TypeEntry *Type_get(TypeGraph *tg, Type handle) {
    if (handle.id > tg->entries.len)
        fungus_panic("attempted to get an invalid type.");

    return tg->entries.data[handle.id];
}

static bool validate_def(TypeGraph *tg, TypeDef *def) {
    // TODO verify def->expr

    bool abstracts_are_abstract = true;

    for (size_t i = 0; i < def->is_len; ++i) {
        if (Type_get(tg, def->is[i])->type != TY_ABSTRACT) {
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

Type Type_define(TypeGraph *tg, TypeDef *def) {
    // validate
    if (!validate_def(tg, def)) {
        fungus_panic("invalid type definition: %.*s",
                     (int)def->name.len, def->name.str);
    }

    // create entry
    TypeEntry *entry = TG_alloc(tg, sizeof(*entry));
    Type handle = (Type){ tg->entries.len };

    *entry = (TypeEntry){
        .name = Word_copy_of(&def->name, &tg->pool),
        .type = def->type
    };

    if (def->type == TY_ABSTRACT) {
        entry->type_set = TG_alloc(tg, sizeof(*entry->type_set));
        *entry->type_set = IdSet_new();
    }

    // place id in abstract type sets
    for (size_t i = 0; i < def->is_len; ++i) {
        TypeEntry *set_entry = Type_get(tg, def->is[i]);

        IdSet_put(set_entry->type_set, handle.id);

        if (entry->type == TY_ABSTRACT)
            IdSet_add_superset(entry->type_set, set_entry->type_set);
    }

    Vec_push(&tg->entries, entry);
    IdMap_put(&tg->by_name, entry->name, handle.id);

    return handle;
}

bool Type_by_name(TypeGraph *tg, Word *name, Type *o_type) {
    return IdMap_get_checked(&tg->by_name, name, &o_type->id);
}

const Word *Type_name(TypeGraph *tg, Type ty) {
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

bool Type_is(TypeGraph *tg, Type ty, Type of) {
    // check self
    if (ty.id == of.id)
        return true;

    TypeEntry *of_entry = Type_get(tg, of);

    return of_entry->type == TY_ABSTRACT
        && IdSet_has(of_entry->type_set, ty.id);
}

bool TypeExpr_is(TypeGraph *tg, TypeExpr *expr, Type of) {
    return TypeExpr_matches(tg, expr, &(TypeExpr){
        .type = TET_ATOM,
        .atom = of
    });
}

bool Type_matches(TypeGraph *tg, Type ty, TypeExpr *pat) {
    TypeEntry *entry = Type_get(tg, ty);

    switch (entry->type) {
    case TY_CONCRETE:
    case TY_ABSTRACT:
        switch (pat->type) {
        case TET_ATOM:
            // whether atom is the other atom
            return Type_is(tg, ty, pat->atom);
        case TET_PRODUCT:
            // atoms can't match products
            return false;
        case TET_SUM:
            // whether atom is included in product
            for (size_t i = 0; i < pat->len; ++i)
                if (Type_matches(tg, ty, pat->exprs[i]))
                    return true;

            return false;
        }
    }
}

bool TypeExpr_matches(TypeGraph *tg, TypeExpr *expr, TypeExpr *pat) {
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

void TypeExpr_print(TypeGraph *tg, TypeExpr *expr) {
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

void Type_print(TypeGraph *tg, Type ty) {
    TypeEntry *entry = Type_get(tg, ty);

    bool symbolic = ispunct(entry->name->str[0]);

    if (symbolic) printf("`");
    printf("%.*s", (int)entry->name->len, entry->name->str);
    if (symbolic) printf("`");
}

void Type_print_verbose(TypeGraph *tg, Type ty) {
    Type_print(tg, ty);

    TypeEntry *entry = Type_get(tg, ty);

    if (entry->type == TY_ABSTRACT) {
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

void TypeGraph_dump(TypeGraph *tg) {
    puts(TC_CYAN "TypeGraph:" TC_RESET);
    puts("abstract:");

    for (size_t i = 0; i < tg->entries.len; ++i) {
        Type ty = { i };

        if (Type_get(tg, ty)->type != TY_ABSTRACT)
            continue;

        printf("  ");
        Type_print_verbose(tg, ty);
        puts("");
    }

    puts("concrete:");
    printf("  ");

    for (size_t i = 0; i < tg->entries.len; ++i) {
        Type ty = { i };

        if (Type_get(tg, ty)->type == TY_ABSTRACT)
            continue;

        Type_print_verbose(tg, ty);
        printf(" ");
    }

    puts("\n");
}
