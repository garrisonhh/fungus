#include <assert.h>

#include "syntax.h"

// TODO I already know that REPEAT and OPTIONAL will be problematic for this
// func
static bool pattern_check(Fungus *fun, Pattern *pat, Expr **slice, size_t len) {
    TypeGraph *types = &fun->types;

    // initialize generics with placeholders
    Type *generics = malloc(pat->where_len * sizeof(*generics));

    for (size_t i = 0; i < pat->where_len; ++i)
        generics[i] = INVALID_TYPE;

    // ensure all generics are consistent
    bool valid = true;

    for (size_t i = 0; i < len; ++i) {
        Expr *expr = slice[i];
        WherePat *where = &pat->where[pat->pat[i]];
        Type *generic = &generics[pat->pat[i]];

        if (generic->id == INVALID_TYPE.id) {
            if (Type_is(types, expr->cty, where->ty))
                *generic = expr->cty;
            else if (Type_is(types, expr->ty, where->ty))
                *generic = expr->ty;
            else
                fungus_panic("??? match was incorrect");
        } else if (generic->id != expr->ty.id && generic->id != expr->cty.id) {
             valid = false;
             break;
        }
    }

    free(generics);

    return valid;
}

/*
 * o_len tracks the best length match found so far. if a longer match is found,
 * it will be modified
 *
 * TODO should I check patterns during this? should generic parameters be
 * tracked through rule matching? might make things easier in the end, only one
 * implementation of 'matching' to keep track of
 */
static RuleEntry *try_match_rule(Fungus *fun, RuleNode *node, Expr **slice,
                                 size_t len, size_t depth, size_t *o_len) {
    if (depth == len) {
        // full slice match up to this node
        if (!node->terminates)
            return NULL;

        RuleEntry *entry = Rule_get(&fun->rules, node->rule);

        if (!pattern_check(fun, &entry->pat, slice, depth))
            return NULL;

        *o_len = depth; // will always be best depth

        return entry;
    }

    RuleEntry *best_match = NULL;

    if (node->terminates) {
        // partial slice match up to this node
        if (depth > *o_len) {
            RuleEntry *pot_match = Rule_get(&fun->rules, node->rule);

            if (pattern_check(fun, &pot_match->pat, slice, depth)) {
                *o_len = depth;
                best_match = pot_match;
            }
        }
    }

    Expr *expr = slice[depth];

    for (size_t i = 0; i < node->nexts.len; ++i) {
        RuleNode *next = node->nexts.data[i];

        // TODO figure out rule matching conditions!!!
        if (Type_is(&fun->types, expr->cty, next->ty)
         || Type_is(&fun->types, expr->ty, next->ty)) {
            RuleEntry *better_match =
                try_match_rule(fun, next, slice, len, depth + 1, o_len);

            if (better_match)
                best_match = better_match;
        }
    }

    return best_match;
}

static Expr *collapse(Fungus *fun, RuleEntry *entry, Expr **exprs, size_t len) {
    // count children
    size_t num_children = 0;

    for (size_t i = 0; i < len; ++i)
        if (exprs[i]->ty.id != fun->t_notype.id)
            ++num_children;

    // create expr
    Expr *expr = Fungus_tmp_expr(fun, num_children);

    size_t child = 0;

    for (size_t i = 0; i < len; ++i)
        if (exprs[i]->ty.id != fun->t_notype.id)
            expr->exprs[child++] = exprs[i];

    expr->ty = entry->hook(fun, expr);
    expr->cty = entry->ty;

    return expr;
}

static Expr *try_match(Fungus *fun, Expr **slice, size_t len, size_t *o_len) {
    *o_len = 0;

    RuleEntry *matched =
        try_match_rule(fun, fun->rules.root, slice, len, 0, o_len);

    if (matched) {
        const Word *name = Type_name(&fun->types, matched->ty);

        printf("matched rule %.*s (%zu)!\n", (int)name->len, name->str, *o_len);

        return collapse(fun, matched, slice, *o_len);
    }

    puts("matched no rules");

    return NULL;
}

// greatly modifies `exprs` array memory
static Expr *parse_slice(Fungus *fun, Expr **exprs, size_t len) {
    bool found_match = true;

    while (found_match) {
        found_match = false;

        for (size_t i = 0; i < len; ++i) {
            // match on sub-slice
            Expr **sub = &exprs[i];
            size_t sub_len = len - i;

            Expr *match;
            size_t match_len;

            while ((match = try_match(fun, sub, sub_len, &match_len))) {
                Expr_dump(fun, match);
                exit(0);

                // stitch sub-slice
            }
        }
    }

    if (len > 1) {
        // TODO better errors
        fungus_error("could not fully match exprs");
        global_error = true;

        return NULL;
    }

    return exprs[0];
}

// exprs are raw TODO intake tokens instead
Expr *parse(Fungus *fun, Expr *exprs, size_t len) {
    Fungus_tmp_clear(fun);

    if (len == 0)
        return NULL;

    // create array of ptrs so parser can swallow it
    Expr **expr_slice = Fungus_tmp_alloc(fun, len * sizeof(*expr_slice));

    for (size_t i = 0; i < len; ++i)
        expr_slice[i] = &exprs[i];

    return parse_slice(fun, expr_slice, len);
}
