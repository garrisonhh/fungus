#include <string.h>

#include "syntax.h"
#include "lex.h"
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

static Expr *Expr_new(IterCtx *ctx, Type ty, MetaType mty) {
    Expr *expr = Bump_alloc(&ctx->pool, sizeof(*expr));

    expr->ty = ty;
    expr->mty = mty;

    return expr;
}

static bool pattern_matches(Pattern *pat, Expr *expr) {
    if (pat->ty.id != TY_NONE)
        if (pat->ty.id != expr->ty.id)
            return false;

    if (pat->mty.id != MTY_NONE)
        if (pat->mty.id != expr->mty.id)
            return false;

    return true;
}

// assumes length of exprs is enough to fit rule
static bool rule_matches(Rule *rule, Expr **exprs) {
    for (size_t i = 0; i < rule->len; ++i)
        if (!pattern_matches(&rule->pattern[i], exprs[i]))
            return false;

    return true;
}

static Expr *construct_rule_expr(IterCtx *ctx, Rule *rule, Expr **slice) {
    // TODO counting children should be done at construction of Rule
    size_t num_children = 0;

    for (size_t i = 0; i < rule->len; ++i)
        if (rule->pattern[i].ty.id != TY_NONE)
            ++num_children;

    // create the new expr
    Expr *expr = Expr_new(ctx, rule->ty, rule->mty);

    expr->len = num_children;
    expr->exprs = Bump_alloc(&ctx->pool, expr->len * sizeof(*expr->exprs));

    for (size_t i = 0, j = 0; i < rule->len; ++i)
        if (rule->pattern[i].ty.id != TY_NONE)
            expr->exprs[j++] = slice[i];

    return expr;
}

/*
 * treeifies a block as much as possible, and then returns its new length.
 *
 * this is truly the guts of the parser. right now this is using a very
 * brute-forcey algorithm, similar to precedence climbing but rather than
 * climbing the precedences we're starting from the top and descending.
 *
 * so definitely a LOT of room for improvement, but it works!
 */
static size_t treeify_block(IterCtx *ctx, Expr **exprs, size_t len) {
    // walk down precedences (highest to lowest) and combine exprs into branches
    for (size_t i = 0; i < syn_tab.rules.len; ++i) {
        Rule *rule = syn_tab.rules.data[i];

        // TODO right associativity -> walk backwards
        if (rule->len <= len) {
            for (size_t j = 0; j <= len - rule->len; ++j) {
                if (rule_matches(rule, &exprs[j])) {
                    // rule matched, store it and modify expr slice
                    exprs[j] = construct_rule_expr(ctx, rule, &exprs[j]);

                    size_t move_size = (len - (j + rule->len)) * sizeof(*exprs);

                    if (move_size > 0)
                        memmove(&exprs[j + 1], &exprs[j + rule->len], move_size);

                    len -= rule->len - 1;

                    // terminate treeification early if no more matches will be
                    // possible
                    if (len <= 1)
                        goto treeify_done;

                    /*
                     * TODO what I really want to do here is restart rule
                     * checking from the first rule in the current precedence
                     * level, and checking each rule in the precedence level
                     * at the same time instead of one at a time.
                     *
                     * this will mean redoing the way rules are stored,
                     * ordering them by precedence instead. for now this hack
                     * works OK-ish, it generates some weird results but not
                     * invalid results.
                     */
                    --j;
                    continue;
                }
            }
        }
    }

treeify_done:
    return len;
}

#define MAX_BLOCK_LVL 1024

// parses Expr tree within a block
static Expr *parse_block(IterCtx *ctx, Expr **slice, size_t len, MetaType mty) {
    // recursively parse sub-blocks
    Expr **exprs = malloc(len * sizeof(*exprs));
    size_t idx = 0;

    size_t match_indices[MAX_BLOCK_LVL];
    BlockRule *matches[MAX_BLOCK_LVL];
    size_t match = 0;

    for (size_t i = 0; i < len; ++i) {
        Expr *cur = slice[i];

        // match any BlockRules TODO hash this or something ffs
        for (size_t j = 0; j < syn_tab.brules.len; ++j) {
            BlockRule *brule = syn_tab.brules.data[j];

            if (cur->mty.id == brule->start.id) {
                // start block
                matches[match] = brule;
                match_indices[match] = i;
                ++match;

                break;
            } else if (cur->mty.id == brule->stop.id) {
                // stop block
                if (match == 0)
                    goto parse_block_error;

                BlockRule *top = matches[--match];

                if (top != brule)
                    goto parse_block_error;

                if (match == 0) {
                    size_t start_idx = match_indices[match];

                    cur = parse_block(ctx, &slice[start_idx + 1],
                                      i - start_idx - 1, brule->mty);
                }

                break;
            }
        }

        if (match == 0)
            exprs[idx++] = cur;
    }

    // treeify this block
    size_t block_len = treeify_block(ctx, exprs, idx);

    // copy exprs to block (once actually parsing all rules this will be
    // unnecessary)
    Expr *expr = Expr_new(ctx, TYPE(TY_NONE), mty);
    size_t exprs_size = idx * sizeof(*expr->exprs);

    expr->exprs = Bump_alloc(&ctx->pool, exprs_size);
    expr->len = block_len;

    memcpy(expr->exprs, exprs, exprs_size);

    return expr;

parse_block_error:
    // TODO more informative errors
    fungus_error("block parsing failed.");
    global_error = true;

    return NULL;
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
