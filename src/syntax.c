#include <assert.h>

#include "syntax.h"

/*
 * TODO after experimentation it is clear that matching on runtypes doesn't make
 * much sense, and type checking expressions using rulehooks doesn't make sense
 * until exprs are fully constructed and associativity is determined. This will
 * be buggy until those problems are resolved for some rules.
 */

/*
 * not very well named. this function ensures exprs with children properly
 * respect associativity, and calls all the right hooks.
 */
static Expr *resolve_childed(Fungus *fun, RuleEntry *entry, Expr *expr) {
    TypeGraph *types = &fun->types;
    PrecGraph *precs = &fun->precedences;
    RuleTree *rules = &fun->rules;

    // collapse is left associative by default, do tree swap to fix
    Expr *left = expr->exprs[0];
    RuleEntry *expr_entry = Rule_get(rules, expr->rule);
    RuleEntry *left_entry = Rule_get(rules, left->rule);

    if (!Type_is(types, left->cty, fun->t_literal)
        && !expr_entry->prefixed && !left_entry->postfixed) {
        Comparison cmp = Prec_cmp(precs, expr_entry->prec, left_entry->prec);

        if (cmp == GT || (cmp == EQ && entry->assoc == ASSOC_RIGHT)) {
            // find rightmost expression of left expr
            Expr **lr = &left->exprs[left->len - 1];
            RuleEntry *lr_entry = Rule_get(rules, (*lr)->rule);

            while (!Type_is(types, (*lr)->cty, fun->t_literal)
                   && !lr_entry->postfixed
                   && Prec_cmp(precs, expr_entry->prec, lr_entry->prec) >= 0) {
                lr = &(*lr)->exprs[(*lr)->len - 1];
                lr_entry = Rule_get(rules, (*lr)->rule);
            }

            // swap tree nodes
            Expr **rl = &expr->exprs[0];

            *rl = *lr;
            *lr = expr;

            // call hooks and return
            entry->hook(fun, entry, expr);

            // TODO must call RuleHook on left expr again, it might have
            // a new outcome

            return left;
        }
    }

    // this expr needs nothing special done!
    entry->hook(fun, entry, expr);

    return expr;
}

/*
 * TODO determining whether another rule actually has a lhs or rhs (like parens
 * never have either) will likely require access to the Rule struct data for
 * another rule, I should allow access to Rule data in the RuleTree from the
 * comptype of an expr!
 */
static Expr *collapse(Fungus *fun, RuleEntry *entry, Expr **exprs, size_t len) {
    TypeGraph *types = &fun->types;

    // alloc expr
    size_t num_children = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->ty, fun->t_notype))
            ++num_children;

    Expr *expr = Fungus_tmp_expr(fun, num_children);

    if (num_children)
        expr->rule = entry->handle;

    // copy children to rule
    size_t j = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->cty, fun->t_lexeme))
            expr->exprs[j++] = exprs[i];

    // fix associativity + call hooks
    if (num_children)
        expr = resolve_childed(fun, entry, expr);
    else
        entry->hook(fun, entry, expr);

    return expr;
}

static bool node_match(Fungus *fun, RuleNode *node, Expr *expr) {
    return Type_is(&fun->types, expr->cty, node->ty)
           || Type_is(&fun->types, expr->ty, node->ty);
}

static RuleEntry *match_r(Fungus *fun, Expr **exprs, size_t len,
                          RuleNode *state, size_t depth, size_t *o_match_len) {
    // check if fully explored exprs, this should always be longest!
    if (len == 0) {
        if (state->terminates)
            *o_match_len = depth;

        return Rule_get(&fun->rules, state->rule);
    }

    // check if this node could be the best match!
    RuleEntry *best_match = NULL;

    if (depth > *o_match_len) {
        if (state->terminates) {
            *o_match_len = depth;

            best_match = Rule_get(&fun->rules, state->rule);
        }
    }

    // match on children
    Expr *expr = exprs[0];

    for (size_t i = 0; i < state->nexts.len; ++i) {
        RuleNode *pot_state = state->nexts.data[i];

        if (node_match(fun, pot_state, expr)) {
            RuleEntry *found = match_r(fun, exprs + 1, len - 1, pot_state,
                                       depth + 1, o_match_len);

            if (found) {
                // full matches should be returned immediately
                if (*o_match_len == depth + len)
                    return found;

                best_match = found;
            }
        }
    }

    return best_match;
}

static RuleEntry *match(Fungus *fun, Expr **exprs, size_t len, size_t *o_match_len) {
    size_t match_len = 0;
    RuleEntry *matched = match_r(fun, exprs, len, fun->rules.root, 0, &match_len);

    if (o_match_len)
        *o_match_len = match_len;

    return matched;
}

// will heavily mutate and invalidate slice
static Expr *parse_slice(Fungus *fun, Expr **exprs, size_t len) {
    while (len > 1) {
        bool found_match = false;

        // iterate through exprs until a rule is found and can be collapsed
        for (size_t i = 0; i < len; ++i) {
            // match on this slice
            Expr **slice = &exprs[i];
            size_t slice_len = len - i;

            size_t match_len;
            RuleEntry *matched = match(fun, slice, slice_len, &match_len);

            // collapse once matched
            if (matched) {
                Expr *collapsed = collapse(fun, matched, slice, match_len);

                // sew up expr array around this collapse
                Expr **sewn_exprs = exprs + match_len - 1;

                for (size_t j = i; j > 0; --j)
                    sewn_exprs[j - 1] = exprs[j - 1];

                sewn_exprs[i] = collapsed;

                exprs = sewn_exprs;
                len -= match_len - 1;

                // break to outer loop
                found_match = true;
                break;
            }
        }

        if (!found_match) {
            // matching unsuccessful!
            fungus_error("no matches found for exprs:");
            Expr_dump_array(fun, exprs, len);
            global_error = true;

            return NULL;
        }
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
