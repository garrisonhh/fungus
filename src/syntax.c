#include <assert.h>

#include "syntax.h"

/*
 * TODO I think my best bet for handling precedence is using tree manipulations
 * to handle operator precedence. operator precedence only actually affects
 * infix-y expressions, so I will just need to recursively delve into the AST
 *
 * TODO determining whether another rule actually has a lhs or rhs (like parens
 * never have either) will likely require access to the Rule struct data for
 * another rule, I should either store Rule pointers in Exprs (bad idea for long
 * term rewriteability) or allow access to Rule data in the RuleTree from the
 * comptype of an expr
 */
static Expr *collapse(Fungus *fun, Rule *rule, Expr **exprs, size_t len) {
    // alloc expr
    TypeGraph *types = &fun->types;
    size_t num_children = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->ty, fun->t_notype))
            ++num_children;

    Expr *expr = Fungus_tmp_expr(fun, num_children);

    // copy children to rule
    size_t j = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->ty, fun->t_notype))
            expr->exprs[j++] = exprs[i];

    // apply rule
    rule->hook(fun, rule, expr);

    return expr;
}

static bool node_match(Fungus *fun, RuleNode *node, Expr *expr) {
    return Type_is(&fun->types, expr->cty, node->ty)
           || Type_is(&fun->types, expr->ty, node->ty);
}

#if 0
// returns terminicity, outputs any rule matches
static bool match_r(Fungus *fun, Expr **exprs, size_t len, RuleNode *state,
                    Rule **o_rule) {
    if (len == 0) {
        *o_rule = state->rule;

        // reached end of tree branch
        return state->rule || !state->nexts.len;
    } else {
        // parse_r on children
        Expr *expr = exprs[0];

        for (size_t i = 0; i < state->nexts.len; ++i) {
            RuleNode *pot_state = state->nexts.data[i];

            // parse_r on this state
            if (node_match(fun, pot_state, expr)
                && match_r(fun, exprs + 1, len - 1, pot_state, o_rule)) {
                return true;
            }
        }

        // terminal failure, this slice will never match a rule!
        return false;
    }
}
#endif

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
    assert(o_match_len);

    *o_match_len = 0;

    return match_r(fun, exprs, len, fun->rules.root, 0, o_match_len);
}

/*
 * TODO it makes much more sense to push rawexprs/tokens on a stack from the
 * right instead of the left. this will eliminate a crapload of recursive
 * function calls and unnecessary failed flat_matching, especially now
 * that I'm doing precedence through tree manipulation.
 *
 * TODO consider matching on a token stream in some form rather than an Expr *
 * array, get those data-oriented gains
 */
static Expr *parse_r(Fungus *fun, Expr **exprs, size_t len,
                     size_t *o_match_len) {
    Expr *stack[MAX_RULE_LEN];
    size_t sp = 0;
    Rule *best_match = NULL;
    size_t best_match_sp = 0, best_match_len = 0;

    size_t idx = 0;

    // always pop and parse_r lhs (first element in rule)
    stack[sp++] = exprs[idx++];

    size_t lhs_match_len;
    Rule *lhs_rule = match(fun, stack, 1, &lhs_match_len);

    if (lhs_rule)
        stack[0] = collapse(fun, lhs_rule, stack, 1);

    // parse rhs
    while (idx < len) {
        // get next expr
        size_t next_match_len;
        Expr *next_expr = parse_r(fun, &exprs[idx], len - idx, &next_match_len);

        assert(next_expr);

        stack[sp++] = next_expr;
        idx += next_match_len;

        // attempt to match a rule
        size_t match_len;
        Rule *matched = match(fun, stack, sp, &match_len);

        if (matched) {
            best_match = matched;
            best_match_sp = sp;
            best_match_len = match_len;
        }

        if (match_len == sp)
            break;
    }

    if (!best_match) {
        // no matches found, return lhs
        *o_match_len = 1;

        return stack[0];
    } else {
        // matched a rule!
        *o_match_len = best_match_len;

        return collapse(fun, best_match, stack, best_match_sp);
    }
}

#if 0
typedef struct PStack {
    Expr *data[MAX_RULE_LEN];
    size_t sp, len;
} PStack;

static PStack PStack_new(void) {
    return (PStack){
        .sp = MAX_RULE_LEN - 1
    };
}

static inline Expr **PStack_raw(PStack *ps) {
    return &ps->data[ps->sp];
}

static void PStack_push(PStack *ps, Expr *expr) {
    ps->data[ps->sp--] = expr;
    ++ps->len;
}

static void PStack_collapse(PStack *ps, Fungus *fun, Rule *rule, size_t len) {
    assert(len < ps->len);

    Expr *collapsed = collapse(fun, rule, PStack_raw(ps), len);

    ps->sp += len;
    ps->len -= len;

    PStack_push(ps, collapsed);
}

static Expr *parse_r2(Fungus *fun, Expr **exprs, size_t len) {
    PStack ps = PStack_new();
}
#endif

// exprs are raw TODO intake tokens instead
Expr *parse(Fungus *fun, Expr *exprs, size_t len) {
    Fungus_tmp_clear(fun);

    // create array of ptrs
    Expr **expr_slice = Fungus_tmp_alloc(fun, len * sizeof(*expr_slice));

    for (size_t i = 0; i < len; ++i)
        expr_slice[i] = &exprs[i];

    // parse recursively
    size_t match_len;
    Expr *ast = parse_r(fun, expr_slice, len, &match_len);

    if (match_len != len) // TODO more intelligent error
        fungus_panic("failed to match the full expression!");

    return ast;
}
