#include <assert.h>

#include "parse.h"

/*
 * o_len tracks the best length match found so far. if a longer match is found,
 * it will be modified
 */
static Rule match_rule(Fungus *fun, RuleNode *node, Expr **slice,
                       size_t len, size_t *o_len) {
}

Expr *parse_next_expr(Fungus *fun, Expr **slice, size_t len, Prec lowest_prec,
                      size_t *o_len) {

}

// exprs are raw TODO intake tokens instead
Expr *parse(Fungus *fun, Expr *exprs, size_t len) {
    // create array of ptrs so parser can swallow it
    Expr **expr_slice = Fungus_tmp_alloc(fun, len * sizeof(*expr_slice));

    for (size_t i = 0; i < len; ++i)
        expr_slice[i] = &exprs[i];

    // parse top exprs from slice
    Vec top_exprs = Vec_new();
    size_t idx = 0;

    while (idx < len) {
        size_t expr_len;
        Vec_push(&top_exprs, parse_next_expr(fun, &expr_slice[idx], len,
                                             fun->p_lowest, &expr_len));

        idx += expr_len;
    }

    puts(TC_CYAN "parsed:" TC_RESET);
    Expr_dump_array(fun, (Expr **)top_exprs.data, top_exprs.len);

    Vec_del(&top_exprs);
    exit(0);
}
