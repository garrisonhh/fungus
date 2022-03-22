#include <assert.h>

#include "parse.h"

static int try_match_calls;

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
    // check if this return type is inferrable via a parameter
    // TODO this could be precomputed in Rule_define and stored in RuleEntry
    for (size_t i = 0; i < pat->len; ++i)
        if (pat->pat[i] == pat->returns)
            return slice[i]->ty;

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
    ++try_match_calls;

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

// whether this expr is atomic to the right
static bool is_right_atomic(Fungus *fun, Expr *expr) {
    return expr->atomic || expr->len <= 1
        || Rule_get(&fun->rules, expr->rule)->prefixed;
}

// whether this expr is atomic to the left
static bool is_left_atomic(Fungus *fun, Expr *expr) {
    return expr->atomic || expr->len <= 1
        || Rule_get(&fun->rules, expr->rule)->postfixed;
}

static Expr *try_rot_right(Fungus *fun, Expr *expr) {
    RuleTree *rules = &fun->rules;
    PrecGraph *precedences = &fun->precedences;

    // check if expr could swap
    if (is_left_atomic(fun, expr))
        return expr;

    Expr **left = Expr_lhs(expr);

    // check if left could swap
    if (is_right_atomic(fun, *left))
        return expr;

    // check if precedence says that swap should happen
    RuleEntry *entry = Rule_get(rules, expr->rule);
    RuleEntry *left_entry = Rule_get(rules, (*left)->rule);
    Comparison cmp = Prec_cmp(precedences, left_entry->prec, entry->prec);

    if (!(cmp == LT || (cmp == EQ && entry->assoc == ASSOC_RIGHT)))
        return expr;

    // find expr that could swap between lhs and rhs
    Expr **swap = left;

    do { swap = Expr_rhs(*swap); } while (!is_right_atomic(fun, *swap));

    // check types to see if swap is possible
    /*
     * TODO this is a bandaid until I figure out a cleaner impl:
     * type pattern re-checking will have to be propagated down the parent to
     * child chain and the output expr will need its type to be recalculated
     */
    if ((*swap)->ty.id != (*left)->ty.id)
        return expr;

    // rotate right!
    Expr *mid = *swap;
    *swap = expr;
    expr = *left;
    *left = mid;

    return expr;
}

static Expr *correct_precedence(Fungus *fun, Expr *expr) {
    assert(!expr->atomic);

    expr = try_rot_right(fun, expr);
    // TODO expr = try_rot_left(fun, expr);

    return expr;
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

    expr = correct_precedence(fun, expr);

    return expr;
}

static Expr *try_match(Fungus *fun, Expr **slice, size_t len, size_t *o_len) {
    *o_len = 0;

    RuleEntry *matched =
        try_match_rule(fun, fun->rules.root, slice, len, 0, o_len);

    if (matched)
        return collapse(fun, matched, slice, *o_len);

    return NULL;
}

// greatly modifies `exprs` array memory
static Expr *parse_slice(Fungus *fun, Expr **exprs, size_t len) {
    try_match_calls = 0;

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
                found_match = true;

                // stitch sub-slice
                size_t stitch_len = match_len - 1;

                for (size_t j = 1; j <= i; ++j)
                    exprs[i + stitch_len - j] = exprs[i - j];

                exprs[i + stitch_len] = collapsed;

                // correct array pointers
                exprs += stitch_len;
                len -= stitch_len;
                sub += stitch_len;
                sub_len -= stitch_len;

#if 1
                puts("post rule collapse:");
                Expr_dump_array(fun, exprs, len);
                puts("");
#endif
            }
        }
    }

    if (len > 1) {
        // TODO better errors
        fungus_error("could not fully match exprs");
        global_error = true;

        return NULL;
    }

    printf("try match calls from parsing: %d\n", try_match_calls);

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
