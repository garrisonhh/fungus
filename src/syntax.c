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

    // collapse is right associative by default, got to swap stuff around
    // for left associative rules! TODO a left-associative collapse would
    // do a lot less work, I think. long term plan tho.
    Expr *ret_expr = expr;

    if (rule->assoc != ASSOC_RIGHT) {
        Expr *right = expr->exprs[expr->len - 1];

        // TODO could cut down on these operations by taking advantage of
        // commutativity for some left associative rules
        if (!Type_is(&fun->types, right->cty, fun->t_literal)
            && !expr->postfixed && !right->prefixed
            && Prec_cmp(precs, expr->prec, right->prec) >= 0) {
            // find leftmost expression of right expr
            Expr **rl = &right->exprs[0];

            while (!Type_is(types, (*rl)->cty, fun->t_literal)
                   && !(*rl)->prefixed
                   && Prec_cmp(precs, expr->prec, (*rl)->prec) >= 0) {
                rl = &(*rl)->exprs[0];
            }

            // swap tree nodes
            Expr **lr = &expr->exprs[expr->len - 1];

            *lr = *rl;
            *rl = expr;
            ret_expr = right;
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

/*
 * TODO I can almost definitely combine collapse and match I think?
 */
static Rule *match(Fungus *fun, Expr **exprs, size_t len, size_t *o_match_len) {
    size_t match_len = 0;
    Rule *matched = match_r(fun, exprs, len, fun->rules.root, 0, &match_len);

    if (o_match_len)
        *o_match_len = match_len;

    return matched;
}

typedef struct PStack {
    Expr *data[MAX_RULE_LEN];
    size_t sp, len;
} PStack;

static PStack PStack_new(void) {
    return (PStack){
        .sp = MAX_RULE_LEN
    };
}

static inline Expr **PStack_raw(PStack *ps) {
    return &ps->data[ps->sp];
}

static void PStack_push(PStack *ps, Expr *expr) {
    ps->data[--ps->sp] = expr;
    ++ps->len;
}

static void PStack_collapse(PStack *ps, Fungus *fun, Rule *rule, size_t len) {
    assert(len <= ps->len);

    Expr *collapsed = collapse(fun, rule, PStack_raw(ps), len);

    ps->sp += len;
    ps->len -= len;

    PStack_push(ps, collapsed);
}

static Expr *parse_slice(Fungus *fun, Expr **exprs, size_t len) {
    PStack ps = PStack_new();

    for (long i = len - 1; i >= 0; --i) {
        // push raw expr, parsing as needed
        Expr *raw = exprs[i];
        Rule *raw_match = match(fun, &raw, 1, NULL);

        if (raw_match)
            raw = collapse(fun, raw_match, &raw, 1);

        PStack_push(&ps, raw);

        // match any rules on the stack and collapse them
        size_t match_len;
        Rule *matched = match(fun, PStack_raw(&ps), ps.len, &match_len);

        if (matched)
            PStack_collapse(&ps, fun, matched, match_len);
    }

    // if PStack length is greater than 1, parsing was unsuccessful in fully
    // matching the exprs (syntax error)!
    if (ps.len > 1) {
        fungus_error("unable to match full expr slice!");

        puts("original expr slice:");
        Expr_dump_array(fun, exprs, len);
        puts("\nafter matching:");
        Expr_dump_array(fun, PStack_raw(&ps), ps.len);
        puts("");

        global_error = true;

        return NULL;
    }

    return PStack_raw(&ps)[0];
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
