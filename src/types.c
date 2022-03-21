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

static TypeEntry *TG_get(TypeGraph *tg, Type handle) {
    if (handle.id > tg->entries.len)
        fungus_panic("attempted to get an invalid type.");

    return tg->entries.data[handle.id];
}

static bool validate_def(TypeGraph *tg, TypeDef *def) {
    bool well_formed_expr = def->type != TY_ALIAS || def->expr != NULL;

    // TODO verify def->expr

    bool abstracts_are_abstract = true;

    for (size_t i = 0; i < def->is_len; ++i) {
        if (TG_get(tg, def->is[i])->type != TY_ABSTRACT) {
            abstracts_are_abstract = false;
            break;
        }
    }

    return well_formed_expr && abstracts_are_abstract;
}

static TypeExpr *deepcopy_expr(TypeGraph *tg, TypeExpr *expr) {
    TypeExpr *copy = TG_alloc(tg, sizeof(*copy));

    copy->type = expr->type;

    switch (expr->type) {
    case TET_ATOM:
        copy->atom = expr->atom;
        break;
#if 0
    case TET_TAGGED:
        copy->child = deepcopy_expr(tg, expr->child);
        /* fallthru */
    case TET_TAG:
        copy->tag = Word_copy_of(expr->tag, &tg->pool);
        break;
#endif
    case TET_SUM:
    case TET_PRODUCT:
        copy->len = expr->len;
        copy->exprs = TG_alloc(tg, copy->len * sizeof(*copy->exprs));

        for (size_t i = 0; i < expr->len; ++i)
            copy->exprs[i] = deepcopy_expr(tg, expr->exprs[i]);

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

    switch (def->type) {
    case TY_CONCRETE: break;
    case TY_ABSTRACT:
        entry->type_set = TG_alloc(tg, sizeof(*entry->type_set));
        *entry->type_set = IdSet_new();
        break;
    case TY_ALIAS:
        entry->expr = deepcopy_expr(tg, def->expr);
        break;
    }

    // place id in abstract type sets
    for (size_t i = 0; i < def->is_len; ++i) {
        TypeEntry *set_entry = TG_get(tg, def->is[i]);

        IdSet_put(set_entry->type_set, handle.id);

        if (entry->type == TY_ABSTRACT)
            IdSet_add_superset(entry->type_set, set_entry->type_set);
    }

    Vec_push(&tg->entries, entry);
    IdMap_put(&tg->by_name, entry->name, handle.id);

    return handle;
}

bool Type_get(TypeGraph *tg, Word *name, Type *o_type) {
    return IdMap_get_checked(&tg->by_name, name, &o_type->id);
}

const Word *Type_name(TypeGraph *tg, Type ty) {
    return TG_get(tg, ty)->name;
}

bool Type_is(TypeGraph *tg, Type ty, Type of) {
    // check self
    if (ty.id == of.id)
        return true;

    TypeEntry *of_entry = TG_get(tg, of);

    return of_entry->type == TY_ABSTRACT
        && IdSet_has(of_entry->type_set, ty.id);
}

bool Type_matches(TypeGraph *tg, Type ty, TypeExpr *pat) {
    TypeEntry *entry = TG_get(tg, ty);

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
    case TY_ALIAS:
        return TypeExpr_matches(tg, entry->expr, pat);
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

static void TypeExpr_dump_r(TypeGraph *tg, TypeExpr *expr) {
    fungus_panic("TODO this");
}

void TypeExpr_dump(TypeGraph *tg, TypeExpr *expr) {
    TypeExpr_dump_r(tg, expr);
    puts("");
}

static void Type_dump_r(TypeGraph *tg, Type ty) {
    TypeEntry *entry = TG_get(tg, ty);

    printf("%.*s", (int)entry->name->len, entry->name->str);

    switch (entry->type) {
    case TY_CONCRETE: break;
    case TY_ALIAS:
        printf(": ");
        TypeExpr_dump_r(tg, entry->expr);
        break;
    case TY_ABSTRACT:
        printf(": { ");

        IdSet *type_set = entry->type_set;
        bool first = true;

        for (size_t i = 0; i < type_set->cap; ++i) {
            if (type_set->filled[i]) {
                if (first)
                    first = false;
                else
                    printf(", ");

                const Word *name = TG_get(tg, (Type){ type_set->ids[i] })->name;

                printf("%.*s", (int)name->len, name->str);
            }
        }

        printf(" }");

        break;
    }
}

void Type_dump(TypeGraph *tg, Type ty) {
    Type_dump_r(tg, ty);
    puts("");
}

void TypeGraph_dump(TypeGraph *tg) {
    term_format(TERM_CYAN);
    puts("TypeGraph:");
    term_format(TERM_RESET);

    for (size_t i = 0; i < tg->entries.len; ++i)
        Type_dump(tg, (Type){ i });

    puts("");
}
