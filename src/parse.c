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
static Vec gen_initial_scope(AstCtx *ctx, const TokBuf *tb) {
    Vec scope = Vec_new();

    for (size_t i = 0; i < TokBuf_len(tb); ++i) {
        Token tok = TokBuf_get(tb, i);
        AstExpr *expr = NULL;

        assert(tok.type != TOK_INVALID);

        switch (tok.type) {
        case TOK_ESCAPE: {
            // consume next token, creating a literal lexeme
            bool invalid = false;

            if (i + 1 == TokBuf_len(tb)) {
                invalid = true;
            } else {
                tok = TokBuf_get(tb, ++i);

                if (tok.type != TOK_LEXEME && tok.type != TOK_IDENT)
                    invalid = true;
            }

            if (invalid) {
                    File_error_at(ctx->file, tok.start, tok.len,
                                  "escape not followed by a lexeme.");
            }

            expr = new_atom(ctx->pool, fun_literal, fun_lexeme, tok.start,
                            tok.len);
            break;
        }
        case TOK_SCOPE:
            expr = new_atom(ctx->pool, fun_scope, fun_raw_scope, tok.start,
                            tok.len);
            break;
        case TOK_LEXEME:
            expr = new_atom(ctx->pool, fun_lexeme, fun_lexeme, tok.start,
                            tok.len);
            break;
        case TOK_IDENT:
            expr = new_atom(ctx->pool, fun_ident, fun_unknown, tok.start,
                            tok.len);
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
            expr = new_atom(ctx->pool, fun_literal, evaltype_of_lit[tok.type],
                            tok.start, tok.len);
            break;
        }
        case TOK_INVALID:
        case TOK_COUNT:
            UNREACHABLE;
        }

        assert(expr != NULL);
        Vec_push(&scope, expr);
    }

    DEBUG_SCOPE(0,
        puts(TC_YELLOW "TRANSLATED TOKENS:" TC_RESET);

        for (size_t i = 0; i < scope.len; ++i) {
            const AstExpr *expr = scope.data[i];

            Type_print(expr->type);
            printf("!");
            Type_print(expr->evaltype);
            printf(TC_GRAY "\t--- " TC_RESET);
            AstExpr_dump(expr, ctx->lang, ctx->file);
        }
    );

    return scope;
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
    return try_match_r(ctx, &ctx->lang->rules.roots, slice, len, 1, o_rule);
}

static void debug_slice(AstCtx *ctx, AstExpr **slice, size_t len,
                        const char *msg, ...){
#ifdef DEBUG
    va_list ap;
    va_start(ap, msg);
    printf(TC_YELLOW "SLICE: " TC_RESET);
    vprintf(msg, ap);
    puts("");
    va_end(ap);

    for (size_t i = 0; i < len; ++i)
        AstExpr_dump(slice[i], ctx->lang, ctx->file);

    puts("");
#endif
}

// parse a slice of unparsed exprs into a tree
// TODO maybe I should allocate a second buffer and do a write/swap algo instead
// of in-place modification to help cache performance? not super important
// in the immediate future
static AstExpr *parse_scope(AstCtx *ctx, AstExpr **orig_slice, size_t len) {
    const Precs *precs = &ctx->lang->precs;
    const RuleTree *rules = &ctx->lang->rules;
    AstExpr **slice = orig_slice;

    Prec prec = Prec_highest(precs);
#ifdef DEBUG
    int iterations = 0;
#endif

    while (true) {
        bool found_match = false;

#ifdef DEBUG
        ++iterations;
#endif

        if (Prec_assoc(precs, prec) == ASSOC_LEFT) {
            for (size_t i = 0; i < len; ) {
                Rule match;
                size_t match_len = try_match(ctx, &slice[i], len - i, &match);

                if (match_len && Rule_get(rules, match)->prec.id == prec.id) {
                    found_match = true;

                    // collapse rule
                    size_t match_diff = match_len - 1;

                    slice[i + match_diff] =
                        rule_copy_of_slice(ctx->pool, rules, match, &slice[i],
                                           match_len);

                    for (int j = i - 1; j >= 0; --j)
                        slice[j + match_diff] = slice[j];

                    slice += match_diff;
                    len -= match_diff;
                } else {
                    ++i;
                }
            }
        } else { // right assoc
            for (int i = len - 1; i >= 0; ) {
                Rule match;
                size_t match_len = try_match(ctx, &slice[i], len - i, &match);

                if (match_len && Rule_get(rules, match)->prec.id == prec.id) {
                    found_match = true;

                    /*
                     * right associativity must check for backwards-reaching
                     * matches: for a rule that looks like `A* B`, if you have
                     * exprs that match `C A A A B`, right will first match the
                     * final `A B`, which is incorrect. this code chunk will
                     * extend the match backwards until it stops matching the
                     * pattern.
                     */
try_back_match:
                    if (i > 0) {
                        Rule back_match;
                        size_t back_match_len =
                            try_match(ctx, &slice[i - 1], len - (i - 1),
                                      &back_match);

                        if (back_match.id == match.id
                         && back_match_len == match_len + 1) {
                            ++match_len;
                            --i;
                            goto try_back_match;
                        }
                    }

                    // collapse rule
                    slice[i] = rule_copy_of_slice(ctx->pool, rules, match,
                                                  &slice[i], match_len);

                    size_t match_diff = match_len - 1;

                    for (size_t j = i + 1; j < len - match_diff; ++j)
                        slice[j] = slice[j + match_diff];

                    len -= match_diff;
                } else {
                    --i;
                }
            }
        }

        if (Prec_is_lowest(prec))
           break;

        // iterate
        if (found_match)
            found_match = false;
        else
            Prec_dec(&prec);
    }

    // return AstExpr block as a scope
    return rule_copy_of_slice(ctx->pool, rules, rules->rule_scope, slice, len);
}

// interface ===================================================================

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
#ifdef DEBUG
    double start;
    size_t start_mem;
#endif

    DEBUG_SCOPE(0,
        start = time_now();
        start_mem = ctx->pool->total;
    );

    Vec scope = gen_initial_scope(ctx, tb);
    AstExpr *ast = parse_scope(ctx, (AstExpr **)scope.data, scope.len);
    Vec_del(&scope);

    assert(ast->type.id == ID_SCOPE && ast->evaltype.id != ID_RAW_SCOPE);

    if (!ast)
        global_error = true;

    DEBUG_SCOPE(0,
        if (ast) {
            printf("ast used/total allocated memory: %zu/%zu\n",
                   ast_used_memory(ast), ctx->pool->total - start_mem);
        }

        double duration = time_now() - start;

        printf("parsing took %.6fs.\n", duration);
    );

    DEBUG_SCOPE(0,
        puts(TC_YELLOW "PARSING:" TC_RESET);
        TokBuf_dump(tb, ctx->file);
        AstExpr_dump(ast, ctx->lang, ctx->file);
        puts("");
    );

    return ast;
}
