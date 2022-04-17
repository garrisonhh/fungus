#include <assert.h>
#include <ctype.h>

#include "types.h"
#include "names.h"

Bump typenames_pool;
Vec typenames; // Vec<Word *>

void types_init(void) {
    typenames_pool = Bump_new();
    typenames = Vec_new();
}

void types_quit(void) {
    Vec_del(&typenames);
    Bump_del(&typenames_pool);
}

const Word *Type_name(Type ty) {
    return typenames.data[ty.id];
}

Type Type_define(Names *names, Word name) {
    Type handle = { typenames.len };

    *Vec_alloc(&typenames) = Word_copy_of(&name, &typenames_pool);

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
    case TET_PRODUCT:
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
    case TET_PRODUCT:
        if (expr->len != other->len)
            return false;

        for (size_t i = 0; i < expr->len; ++i)
            if (!TypeExpr_deepequals(expr->exprs[i], other->exprs[i]))
                return false;

        return true;
    }
}

// this exists for planned abstract types
bool Type_is(Type ty, Type of) {
    return ty.id == of.id;
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
    case TET_PRODUCT:
        // atoms can't match products
        return false;
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
    case TET_PRODUCT:
        switch (pat->type) {
        case TET_ATOM:
            // products can't match atoms
            return false;
        case TET_SUM:
            // products could match something in a sum
            for (size_t i = 0; i < pat->len; ++i)
                if (TypeExpr_matches(expr, pat->exprs[i]))
                    return true;

            return false;
        case TET_PRODUCT:
            // whether all of expr members match pat's members
            if (pat->len != expr->len)
                return false;

            for (size_t i = 0; i < expr->len; ++i)
                if (!TypeExpr_matches(expr->exprs[i], pat->exprs[i]))
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

void TypeExpr_print(const TypeExpr *expr) {
    switch (expr->type) {
    case TET_ATOM:
        printf(TC_BLUE);
        Type_print(expr->atom);
        printf(TC_RESET);
        break;
    case TET_SUM:
        for (size_t i = 0; i < expr->len; ++i) {
            bool add_parens = expr->exprs[i]->type == TET_PRODUCT;

            if (i) printf(" | ");
            if (add_parens) printf("(");
            TypeExpr_print(expr->exprs[i]);
            if (add_parens) printf(")");
        }
        break;
    case TET_PRODUCT:
        for (size_t i = 0; i < expr->len; ++i) {
            if (i) printf(" * ");
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
