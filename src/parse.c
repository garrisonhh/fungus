#include <assert.h>

#include "parse.h"
#include "fungus.h"
#include "lang/ast_expr.h"
#include "lex/lex_strings.h"

static AstExpr *new_atom(Bump *pool, Type type, Type evaltype,
                         hsize_t start, hsize_t len) {
    AstExpr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (AstExpr){
        .type = type,
        .evaltype = evaltype,
        .tok_start = start,
        .tok_len = len
    };

    return expr;
}

static AstExpr *new_rule(Bump *pool, const RuleTree *rt, Rule rule,
                         AstExpr **exprs, size_t len) {
    AstExpr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (AstExpr){
        .type = Rule_typeof(rt, rule),
        .evaltype = fun_unknown,
        .rule = rule,
        .exprs = exprs,
        .len = len
    };

    return expr;
}

static AstExpr *rule_copy_of_slice(Bump *pool, const RuleTree *rt, Rule rule,
                                   AstExpr **slice, size_t len) {
    AstExpr **exprs = Bump_alloc(pool, len * sizeof(*exprs));

    for (size_t j = 0; j < len; ++j)
        exprs[j] = slice[j];

    return new_rule(pool, rt, rule, exprs, len);
}

// turns tokens -> scope of AstExprs
static AstExpr *gen_initial_scope(AstCtx *ctx, const TokBuf *tb) {
    Vec list = Vec_new();

    for (size_t i = 0; i < tb->len; ++i) {
        TokType toktype = tb->types[i];
        hsize_t start = tb->starts[i], len = tb->lens[i];

        AstExpr *expr = NULL;

        assert(toktype != TOK_INVALID);

        switch (toktype) {
        case TOK_ESCAPE:
            // consume next token, creating a literal lexeme
            if (i == tb->len
             || (tb->types[i + 1] != TOK_SYMBOL
              && tb->types[i + 1] != TOK_WORD)) {
                File_error_at(ctx->file, start, len,
                              "escape not followed by a lexeme.");
            }

            ++i;
            expr = new_atom(ctx->pool, fun_literal, fun_lexeme, tb->starts[i],
                            tb->lens[i]);
            break;
        case TOK_SCOPE:
            // TODO
            UNIMPLEMENTED;

            break;
        case TOK_SYMBOL:
        case TOK_WORD:
            // raw lexeme
            expr = new_atom(ctx->pool, fun_lexeme, fun_lexeme, start, len);
            break;
        case TOK_BOOL:
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_STRING: {
            Type evaltype_of_lit[TOK_COUNT] = {
                [TOK_BOOL] = fun_bool,
                [TOK_INT] = fun_int,
                [TOK_FLOAT] = fun_float,
                [TOK_STRING] = fun_string,
            };

            // literal; direct token -> expr translation
            expr = new_atom(ctx->pool, fun_literal, evaltype_of_lit[toktype],
                            start, len);
            break;
        }
        default: UNREACHABLE;
        }

        assert(expr != NULL);
        Vec_push(&list, expr);
    }

    // create scope expr
    const RuleTree *rules = &ctx->lang->rules;
    AstExpr *expr = rule_copy_of_slice(ctx->pool, rules, rules->rule_scope,
                                       (AstExpr **)list.data, list.len);

    Vec_del(&list);

    return expr;
}

static size_t try_match_r(AstCtx *ctx, const Vec *nexts, AstExpr **slice,
                          size_t len, size_t depth, Rule *o_rule) {
    if (len == 0)
        return 0;

    size_t best_depth = 0;
    Rule rule = {0};

    for (size_t i = 0; i < nexts->len; ++i) {
        const RuleNode *node = nexts->data[i];

        if (MatchAtom_matches_rule(ctx->file, node->pred, slice[0])) {
            // match node children
            Rule child_rule;
            size_t child_depth = try_match_r(ctx, &node->nexts, slice + 1,
                                             len - 1, depth + 1, &child_rule);

            if (child_depth > best_depth) {
                rule = child_rule;
                best_depth = child_depth;
            }

            // match node
            if (node->has_rule && best_depth < depth) {
                rule = node->rule;
                best_depth = depth;
            }
        }
    }

    *o_rule = rule;

    return best_depth;
}

// tries to match a rule on a slice, returns length of rule matched
static size_t try_match(AstCtx *ctx, AstExpr **slice, size_t len,
                        Rule *o_rule) {
    size_t match_len =
        try_match_r(ctx, &ctx->lang->rules.roots, slice, len, 1, o_rule);

    return match_len;
}

typedef struct MatchSlice {
    size_t start, len;
    Rule rule;
} MatchSlice;

// collapses matches of slice to left, modifies len to reflect collapse
static void collapse_left(AstCtx *ctx, AstExpr **slice, size_t *io_len,
                          const MatchSlice *matches, size_t num_matches) {
    const RuleTree *rules = &ctx->lang->rules;

    size_t buf_len = *io_len, new_len = 0;
    size_t shrinkage = 0, consumed = 0;

    for (size_t i = 0; i < num_matches; ++i) {
        const MatchSlice *match = &matches[i], *prev = &matches[i - 1];

        while (consumed < match->start + match->len)
            slice[new_len++] = slice[consumed++];

        // guard against match slice conflicts
        size_t shrunk_start = match->start - shrinkage;

        if (i > 0 && prev->start + prev->len > match->start) {
            Rule rematch;
            size_t rematch_len =
                try_match(ctx, &slice[shrunk_start], match->len, &rematch);

            if (rematch_len != match->len || rematch.id != match->rule.id) {
                // this rule has been made invalid by last collapse
                continue;
            }
        }

        // collapse
        slice[shrunk_start] =
            rule_copy_of_slice(ctx->pool, rules, match->rule,
                               &slice[shrunk_start], match->len);

        size_t len_diff = match->len - 1;

        shrinkage += len_diff;
        new_len -= len_diff;
    }

    // consume to end of slice
    while (consumed < buf_len)
        slice[new_len++] = slice[consumed++];

    *io_len = new_len;
}

// collapses left matches of slice to right, modifies len to reflect collapse
static void collapse_right(AstCtx *ctx, AstExpr **slice, size_t *io_len,
                           const MatchSlice *matches, size_t num_matches) {
    const RuleTree *rules = &ctx->lang->rules;

    size_t buf_len = *io_len;
    size_t unconsumed = buf_len;

    AstExpr **stack = slice + buf_len;
    size_t new_len = 0;

    for (size_t i = 0; i < num_matches; ++i) {
        const MatchSlice *match = &matches[num_matches - i - 1];
        const MatchSlice *prev = &matches[num_matches - i];

        while (unconsumed > match->start) {
            *--stack = slice[--unconsumed];
            ++new_len;
        }

        // guard against match slice conflicts
        if (i > 0 && prev->start < match->start + match->len) {
            Rule rematch;
            size_t rematch_len =
                try_match(ctx, stack, match->len, &rematch);

            if (rematch_len != match->len || rematch.id != match->rule.id) {
                // rule was made invalid by last collapse
                continue;
            }
        }

        // collapse
        size_t len_diff = match->len - 1;

        stack[len_diff] = rule_copy_of_slice(ctx->pool, rules, match->rule,
                                             stack, match->len);

        stack += len_diff;
        new_len -= len_diff;
    }

    // consume to end of slice
    while (unconsumed > 0) {
        *--stack = slice[--unconsumed];
        ++new_len;
    }

    // memmove from end of buf to start of buf
    memmove(slice, stack, new_len * sizeof(*slice));

    *io_len = new_len;
}

/*
 * the parsing algorithm is dumb simple:
 *
 * 1. iterate through a slice, matching any rules, store the matched rules and
 *    the highest precedence
 * 2. if there were rule matches, collapse all highest precedence rules and
 *    repeat from step 1
 * 3. otherwise, return the slice
 *
 * this algorithm removes all need for messing with precedence, edge cases with
 * rule type checking, etc. though it's probably a lot less efficient than
 * algorithms which do that
 */
static AstExpr **parse_slice(AstCtx *ctx, AstExpr **slice, size_t len,
                             size_t *o_len) {
    const RuleTree *rules = &ctx->lang->rules;
    const Precs *precs = &ctx->lang->precs;

    // construct buffers for slice matching
    AstExpr **buf = malloc(len * sizeof(*slice));
    size_t buf_len = len;

    memcpy(buf, slice, len * sizeof(*buf));

    MatchSlice *matches = malloc(len * sizeof(*matches));
    size_t num_matches;

    // match algo (as described above)
    while (true) {
        // find all matches at the highest precedence possible
        Prec highest = {0};

        num_matches = 0;

        for (size_t i = 0; i < buf_len; ++i) {
            Rule matched;
            size_t match_len = try_match(ctx, &buf[i], buf_len - i, &matched);

            if (match_len > 0) {
                Prec rule_prec = Rule_get(rules, matched)->prec;
                int cmp = Prec_cmp(rule_prec, highest);

                // if this is now the highest precedence, reset match list
                if (!num_matches || cmp > 0) {
                    highest = rule_prec;
                    num_matches = 0;
                } else if (cmp < 0) {
                    // this match is below the highest precedence
                    continue;
                }

                // if this match is contained by a greedy previous match, ignore
                // it. this can happen with `repeating` rules
                const MatchSlice *prev = &matches[num_matches - 1];

                if (num_matches > 0 && prev->rule.id == matched.id
                 && prev->start + prev->len >= i + match_len) {
                    continue;
                }

                matches[num_matches++] = (MatchSlice){
                    .start = i,
                    .len = match_len,
                    .rule = matched
                };
            }
        }

        // if no matches are found, parsing of slice is done
        if (!num_matches)
            break;

        // collapse all matches
        if (Prec_assoc(precs, highest) == ASSOC_RIGHT)
            collapse_right(ctx, buf, &buf_len, matches, num_matches);
        else
            collapse_left(ctx, buf, &buf_len, matches, num_matches);
    }

    // copy parsed buffer to bump-allocated slice
    AstExpr **parsed = Bump_alloc(ctx->pool, buf_len * sizeof(*buf));

    memcpy(parsed, buf, buf_len * sizeof(*parsed));

    // cleanup and return TODO custom arena allocators
    free(matches);
    free(buf);

    *o_len = buf_len;

    return parsed;
}

// given a raw scope, parse it using a lang
AstExpr *parse_scope(AstCtx *ctx, AstExpr *expr) {
    /* TODO is this obsolete?
    if (!translate_scope(ctx, expr))
        return NULL;
    */

    size_t len;
    AstExpr **slice = parse_slice(ctx, expr->exprs, expr->len, &len);

    expr->exprs = slice;
    expr->len = len;

    return expr;
}

// general =====================================================================

static size_t ast_used_memory(AstExpr *expr) {
    size_t used = sizeof(*expr);

    if (!AstExpr_is_atom(expr)) {
        used += expr->len * sizeof(*expr->exprs);

        for (size_t i = 0; i < expr->len; ++i)
            used += ast_used_memory(expr->exprs[i]);
    }

    return used;
}

AstExpr *parse(AstCtx *ctx, const TokBuf *tb) {
#if 1
    double start = time_now();
#endif

#ifdef DEBUG
    size_t start_mem = ctx->pool->total;
#endif

    AstExpr *ast = parse_scope(ctx, gen_initial_scope(ctx, tb));

    if (!ast)
        global_error = true;

#ifdef DEBUG
    if (ast) {
        printf("ast used/total allocated memory: %zu/%zu\n",
               ast_used_memory(ast), ctx->pool->total - start_mem);
    }
#endif

#if 1
    double duration = time_now() - start;

    printf("parsing took %.6fs.\n", duration);
#endif

    return ast;
}
