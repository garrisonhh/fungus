#include <assert.h>

#include "syntax.h"

// TODO this but with type exprs
static bool pattern_check(Fungus *fun, Pattern *pat, Expr **slice, size_t len) {
    // initialize generics with placeholders
    Type *generics = malloc(pat->where_len * sizeof(*generics));

    for (size_t i = 0; i < pat->where_len; ++i)
        generics[i] = fun->t_notype;

    // ensure all generics are consistent
    bool valid = true;

    for (size_t i = 0; i < len; ++i) {
        Expr *expr = slice[i];
        Type *generic = &generics[pat->pat[i]];

        if (generic->id == fun->t_notype.id) {
            *generic = expr->ty;
        } else if (generic->id != expr->ty.id) {
            // inconsistent types
            valid = false;
            break;
        }
    }

    free(generics);

    return valid;
}

static Type find_return_type(Fungus *fun, Pattern *pat, Expr **slice,
                             size_t len) {
    printf("ARRAY:");

    for (size_t i = 0; i < len; ++i) {
        printf(" ");
        Type_print(&fun->types, slice[i]->ty);
    }

    puts("");

    // check if this return type is inferrable via a parameter
    // TODO this could be stored in RuleEntry
    for (size_t i = 0; i < pat->len; ++i)
        if (pat->pat[i] == pat->returns)
            return slice[i]->ty;

    printf("INFERRING!\n");

    // otherwise it is a concrete unique type from the where clause
    // TODO check this in Rule_define
    TypeExpr *ret_te = pat->where[pat->returns];

    assert(ret_te->type == TET_ATOM);

    return ret_te->atom;
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

        if (Type_matches(&fun->types, expr->ty, next->te)) {
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
        if (Type_is(&fun->types, exprs[i]->ty, fun->t_runtype))
            ++num_children;

    // create expr
    Expr *expr = Fungus_tmp_expr(fun, num_children);

    size_t child = 0;

    for (size_t i = 0; i < len; ++i)
        if (Type_is(&fun->types, exprs[i]->ty, fun->t_runtype))
            expr->exprs[child++] = exprs[i];

    expr->atomic = false;
    expr->rule = entry->rule;
    expr->ty = find_return_type(fun, &entry->pat, exprs, len);

    return expr;
}

static Expr *try_match(Fungus *fun, Expr **slice, size_t len, size_t *o_len) {
    static size_t calls = 0;

    printf("try_match %zu\n", ++calls);

    *o_len = 0;

    RuleEntry *matched =
        try_match_rule(fun, fun->rules.root, slice, len, 0, o_len);

    if (matched) {
        const Word *name = Type_name(&fun->types, matched->ty);
        printf("matched rule %.*s on:\n", (int)name->len, name->str);
        Expr_dump_array(fun, slice, *o_len);
        puts("");

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

            Expr *collapsed;
            size_t match_len;

            while ((collapsed = try_match(fun, sub, sub_len, &match_len))) {
                // stitch sub-slice
                size_t stitch_len = match_len - 1;

                for (size_t j = 0; j < i; ++j)
                    exprs[i + (stitch_len - 1) - j] = exprs[i - j];

                exprs[i + stitch_len] = collapsed;

                // correct array pointers
                exprs += stitch_len;
                len -= stitch_len;
                sub += stitch_len;
                sub_len -= stitch_len;
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
