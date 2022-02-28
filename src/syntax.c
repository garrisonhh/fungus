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
 * metatype of an expr
 */
static Expr *flat_collapse(Fungus *fun, Rule *rule, Expr **exprs, size_t len) {
    // alloc expr
    TypeGraph *types = &fun->types;
    Expr *expr = Fungus_tmp_alloc(fun, sizeof(*expr));

    *expr = (Expr){
        .ty = rule->ty,
        .mty = rule->mty
    };

    // count runtime vals
    size_t num_runtime = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->ty, fun->t_notype))
            ++num_runtime;

    // collapse rule
    expr->len = num_runtime;
    expr->exprs = Fungus_tmp_alloc(fun, expr->len * sizeof(*expr->exprs));

    size_t j = 0;

    for (size_t i = 0; i < len; ++i)
        if (!Type_is(types, exprs[i]->ty, fun->t_notype))
            expr->exprs[j++] = exprs[i];

    return expr;
}

static bool node_match(Fungus *fun, RuleNode *node, Expr *expr) {
    return Type_is(&fun->types, expr->mty, node->ty)
           || Type_is(&fun->types, expr->ty, node->ty);
}

#define VERBOSE_FLAT_MATCH

/*
 * TODO RuleTree matching is a tree search, and I need to implement an actual
 * tree search algorithm. right now `flat_match` cannot recursively backtrack or do any
 * actual tree stuff.
 */
// returns whether the rule expansion was terminal, and outputs a rule
static bool flat_match(Fungus *fun, Expr **exprs, size_t len, Rule **o_rule) {
    assert(len > 0);

#ifdef VERBOSE_FLAT_MATCH
    puts(">>> flat matching >>>");

    for (size_t i = 0; i < len; ++i)
        Expr_dump(fun, exprs[i]);

    puts("<<< flat matching <<<");
#endif

    RuleNode *state = fun->rules.root;

    for (size_t i = 0; i < len; ++i) {
        Expr *expr = exprs[i];

        bool matched = false;

#ifdef VERBOSE_FLAT_MATCH
        printf("matching expr: ");
        Fungus_print_types(fun, expr->ty, expr->mty);
        printf("\n");
#endif

        for (size_t j = 0; j < state->nexts.len; ++j) {
            RuleNode *pot_state = state->nexts.data[j];

#ifdef VERBOSE_FLAT_MATCH
            const Word *tn = Type_name(&fun->types, pot_state->ty);

            printf("  %.*s ? ", (int)tn->len, tn->str);
#endif

            if (node_match(fun, pot_state, expr)) {
                state = pot_state;
                matched = true;
                break;
            }

#ifdef VERBOSE_FLAT_MATCH
            printf("no match\n");
#endif
        }

#ifdef VERBOSE_FLAT_MATCH
        printf("%s\n", matched ? "matched!!!" : "no matches :(");
#endif

        // could not match the array
        if (!matched) {
            *o_rule = NULL;

#ifdef VERBOSE_FLAT_MATCH
            puts("early terminal failure");
#endif

            return true;
        }
    }

    // may have successfully found a rule
    *o_rule = state->rule;

#ifdef VERBOSE_FLAT_MATCH
    if (state->nexts.len)
        printf("non-terminal");
    else
        printf("terminal");

    if (state->rule)
        printf(" success\n");
    else
        printf(" faiilure\n");
#endif

    return state->nexts.len == 0;
}

#define MATCH_STACK_SIZE 256

/*
 * TODO consider matching on a token stream in some form rather than an Expr *
 * array, get those data-oriented gains
 */
static Expr *match(Fungus *fun, Expr **exprs, size_t len, size_t *o_match_len) {
    Expr *stack[MATCH_STACK_SIZE];
    size_t sp = 0;
    Rule *best_match = NULL;
    size_t best_match_sp = 0, best_match_len = 0;

    size_t idx = 0;

    // always pop lhs (first element in rule)
    stack[sp++] = exprs[idx++];

    // parse rhs
    while (idx < len) {
        // get next expr
        size_t next_match_len;
        Expr *next_expr = match(fun, &exprs[idx], len - idx, &next_match_len);

        assert(next_expr);

        stack[sp++] = next_expr;
        idx += next_match_len;

        // attempt to match a rule
        Rule *matched = NULL;
        bool terminal = flat_match(fun, stack, sp, &matched);

        if (matched) {
            best_match = matched;
            best_match_sp = sp;
            best_match_len = idx;
        }

        if (terminal)
            break;
    }

    if (!best_match) {
        // no matches found, return lhs
        *o_match_len = 1;

        return stack[0];
    } else {
        *o_match_len = best_match_len;

        return flat_collapse(fun, best_match, stack, best_match_sp);
    }
}

// TODO factor out this function, potentially by making a dummy RuleNode for
// roots, and then just calling match() directly
static Expr *match_roots(Fungus *fun, Expr **exprs, size_t len) {
    size_t match_len;
    Expr *ast = match(fun, exprs, len, &match_len);

    if (match_len != len) // TODO more intelligent error
        fungus_panic("failed to match the full expression!");

    return ast;
}

// exprs are raw TODO intake tokens instead
Expr *parse(Fungus *fun, Expr *exprs, size_t len) {
    Fungus_tmp_clear(fun);

    // create array of ptrs
    Expr **expr_slice = Fungus_tmp_alloc(fun, len * sizeof(*expr_slice));

    for (size_t i = 0; i < len; ++i)
        expr_slice[i] = &exprs[i];

    return match_roots(fun, expr_slice, len);
}
