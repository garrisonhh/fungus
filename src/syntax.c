#include <string.h>

#include "syntax.h"
#include "precedence.h"

// TODO should I unify this somehow with normal pattern rules?
typedef struct BlockSyntaxRule {
    MetaType mty, start, stop;
} BlockRule;

typedef struct SyntaxTable {
    Bump pool;
    Vec rules, brules;
} SyntaxTable;

static SyntaxTable syn_tab;
static MetaType generic_block_ty; // TODO remove

void syntax_init(void) {
    syn_tab.pool = Bump_new();
    syn_tab.rules = Vec_new();
    syn_tab.brules = Vec_new();

    Word generic_name = WORD("generic-block");

    generic_block_ty = metatype_define(&generic_name);

    // parenthetical block
    Word lparen_sym = WORD("(");
    Word rparen_sym = WORD(")");

    MetaType lparen_ty = lex_def_symbol(&lparen_sym);
    MetaType rparen_ty = lex_def_symbol(&rparen_sym);

    Word parens_name = WORD("parens");

    syntax_def_block(&parens_name, lparen_ty, rparen_ty);
}

void syntax_quit(void) {
    Vec_del(&syn_tab.brules);
    Vec_del(&syn_tab.rules);
    Bump_del(&syn_tab.pool);
}

void syntax_dump(void) {
    ; // TODO
}

MetaType syntax_def_block(Word *name, MetaType start, MetaType stop) {
    BlockRule *brule = Bump_alloc(&syn_tab.pool, sizeof(*brule));

    *brule = (BlockRule){
        .mty = metatype_define(name),
        .start = start,
        .stop = stop
    };

    Vec_push(&syn_tab.brules, brule);

    return brule->mty;
}

// sorts by precedence
static int rule_cmp(const void *a, const void *b) {
    return prec_cmp((*(Rule **)a)->prec, (*(Rule **)b)->prec);
}

void syntax_def_rule(Rule *rule) {
    // deepcopy
    Rule *copy = Bump_alloc(&syn_tab.pool, sizeof(*copy));

    *copy = *rule;

    size_t pat_size = rule->len * sizeof(*rule->pattern);

    copy->pattern = Bump_alloc(&syn_tab.pool, pat_size);
    memcpy(copy->pattern, rule->pattern, pat_size);

    // store
    Vec_push(&syn_tab.rules, copy);
    Vec_qsort(&syn_tab.rules, rule_cmp); // TODO less stupid solution
}

// parsing =====================================================================

IterCtx IterCtx_new(void) {
    return (IterCtx){
        .pool = Bump_new()
    };
}

void IterCtx_del(IterCtx *ctx) {
    Bump_del(&ctx->pool);
}

static Expr *alloc_expr(IterCtx *ctx, Type ty, MetaType mty) {
    Expr *expr = Bump_alloc(&ctx->pool, sizeof(*expr));

    expr->ty = ty;
    expr->mty = mty;

    return expr;
}

#define MAX_BLOCK_LVL 1024

// parses Expr tree within a block
static Expr *parse_block(IterCtx *ctx, Expr **slice, size_t len, MetaType mty) {
    // TODO incorporate rule tree
    fungus_panic("treeification with RuleTrees is not implemented atm.");
}

Expr *syntax_parse(IterCtx *ctx, Expr *exprs, size_t len) {
    Expr **slice = malloc(len * sizeof(*exprs));

    for (size_t i = 0; i < len; ++i)
        slice[i] = &exprs[i];

    Expr *expr = parse_block(ctx, slice, len, generic_block_ty);

    Expr_dump(expr);

    free(slice);

    return expr;
}
