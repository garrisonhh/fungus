#include <assert.h>

#include "syntax.h"

/*
 * TODO determining whether another rule actually has a lhs or rhs (like parens
 * never have either) will likely require access to the Rule struct data for
 * another rule, I should either store Rule pointers in Exprs (bad idea for long
 * term rewriteability) or allow access to Rule data in the RuleTree from the
 * comptype of an expr
 */
static Expr *collapse(Fungus *fun, Rule *rule, Expr **exprs, size_t len) {
    TypeGraph *types = &fun->types;
    PrecGraph *precs = &fun->precedences;

    // alloc expr
    size_t num_children = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->ty, fun->t_notype))
            ++num_children;

    Expr *expr = Fungus_tmp_expr(fun, num_children);

    expr->prec = rule->prec;
    expr->prefixed = rule->prefixed;
    expr->postfixed = rule->postfixed;

    // copy children to rule
    size_t j = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->cty, fun->t_lexeme))
            expr->exprs[j++] = exprs[i];

    // collapse is left associative by default, do tree swap to fix!
    Expr *ret_expr = expr;
    Expr *left = expr->exprs[0];

    if (!Type_is(&fun->types, left->cty, fun->t_literal)
        && !expr->prefixed && !left->postfixed) {
        Comparison cmp = Prec_cmp(precs, expr->prec, left->prec);

        if (cmp == GT || (cmp == EQ && rule->assoc == ASSOC_RIGHT)) {
            // find rightmost expression of left expr
            Expr **lr = &left->exprs[left->len - 1];

            while (!Type_is(types, (*lr)->cty, fun->t_literal)
                   && !(*lr)->postfixed
                   && Prec_cmp(precs, expr->prec, (*lr)->prec) >= 0) {
                lr = &(*lr)->exprs[(*lr)->len - 1];
            }

            // swap tree nodes
            Expr **rl = &expr->exprs[0];

            *rl = *lr;
            *lr = expr;
            ret_expr = left;
        }
    }

    // apply rule
    rule->hook(fun, rule, expr);

    return ret_expr;
}

static bool node_match(Fungus *fun, RuleNode *node, Expr *expr) {
    return Type_is(&fun->types, expr->cty, node->ty)
           || Type_is(&fun->types, expr->ty, node->ty);
}

static Rule *match_r(Fungus *fun, Expr **exprs, size_t len, RuleNode *state,
                     size_t depth, size_t *o_match_len) {
    // check if fully explored exprs, this should always be longest!
    if (len == 0) {
        if (state->rule)
            *o_match_len = depth;

        return state->rule;
    }

    // check if this node could be the best match!
    Rule *best_match = NULL;

    if (depth > *o_match_len) {
        if (state->rule)
            *o_match_len = depth;

        best_match = state->rule;
    }

    // match on children
    Expr *expr = exprs[0];

    for (size_t i = 0; i < state->nexts.len; ++i) {
        RuleNode *pot_state = state->nexts.data[i];

        if (node_match(fun, pot_state, expr)) {
            Rule *found = match_r(fun, exprs + 1, len - 1, pot_state, depth + 1,
                                  o_match_len);

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

static Rule *match(Fungus *fun, Expr **exprs, size_t len, size_t *o_match_len) {
    size_t match_len = 0;
    Rule *matched = match_r(fun, exprs, len, fun->rules.root, 0, &match_len);

    if (o_match_len)
        *o_match_len = match_len;

    return matched;
}

// will heavily mutate and invalidate slice
static Expr *parse_slice(Fungus *fun, Expr **exprs, size_t len) {
    while (len > 1) {
        size_t match_len;
        Rule *matched = match(fun, exprs, len, &match_len);

        if (matched) {
            // collapse
            Expr *collapsed = collapse(fun, matched, exprs, match_len);

            exprs += match_len - 1;
            len -= match_len - 1;
            exprs[0] = collapsed;
        } else {
            // no match found, error!
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
