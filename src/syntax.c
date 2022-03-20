#include <assert.h>

#include "syntax.h"

// o_len tracks the best length match found so far. if a longer match is found,
// it will be modified
static RuleEntry *try_match_rule(Fungus *fun, RuleNode *node, Expr **slice,
                                 size_t len, size_t depth, size_t *o_len) {
    if (len == 0) {
        // full slice match up to this node
        if (node->terminates) {
            *o_len = depth; // will always be best depth

            // TODO check pattern

            return Rule_get(&fun->rules, node->rule);
        }

        return NULL;
    }

    RuleEntry *best_match = NULL;

    if (node->terminates) {
        // partial slice match up to this node
        if (depth > *o_len) {
            *o_len = depth;

            // TODO check pattern

            best_match = Rule_get(&fun->rules, node->rule);
        }
    }

    Expr *expr = slice[0];

    for (size_t i = 0; i < node->nexts.len; ++i) {
        RuleNode *next = node->nexts.data[i];

        // TODO figure out rule matching conditions!!!
        if (Type_is(&fun->types, expr->cty, next->ty)
         || Type_is(&fun->types, expr->ty, next->ty)) {
            RuleEntry *better_match =
                try_match_rule(fun, next, slice + 1, len - 1, depth + 1, o_len);

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

    expr->ty = fun->t_runtype; // TODO figure out ty using pattern
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
