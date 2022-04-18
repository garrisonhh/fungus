#include <assert.h>

#include "parse.h"
#include "fungus.h"
#include "lang/ast_expr.h"
#include "lex/char_classify.h"

/*
 * parsing works in two stages:
 * 1. turn scopes (`{ ... }`) into a tree. this is the only parsing rule that is
 *    truly hardcoded into Fungus
 * 2. recursively parse scopes given a language context. this requires
 *    integration with semantic analysis, because `language`s must be parsed
 *    before they can be used for parsing
 */

// stage 1: scope tree =========================================================

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

// turns tokens -> list of exprs, separating `{` and `}` as symbols but not
// creating the tree yet
static void gen_flat_list(AstCtx *ctx, Vec *list, const TokBuf *tb) {
    Type evaltype_of_lit[TOK_COUNT] = {
        [TOK_BOOL] = fun_bool,
        [TOK_INT] = fun_int,
        [TOK_FLOAT] = fun_float,
        [TOK_STRING] = fun_string,
    };

    for (size_t i = 0; i < tb->len; ++i) {
        TokType toktype = tb->types[i];
        hsize_t start = tb->starts[i], len = tb->lens[i];

        assert(toktype != TOK_INVALID);

        if (toktype == TOK_SYMBOLS) {
            const char *text = &tb->file->text.str[start];

            if (text[0] == '`') {
                // escaped literal lexeme
                if (len == 1)
                    File_error_at(tb->file, start, len, "bare lexeme escape.");

                AstExpr *expr =
                    new_atom(ctx->pool, fun_literal, fun_lexeme, start, len);

                Vec_push(list, expr);
            } else {
                // split symbols on `{` and `}`
                hsize_t last_split = 0;

                for (hsize_t j = 0; j < len; ++j) {
                    if (text[j] == '{' || text[j] == '}') {
                        if (j > last_split) {
                            AstExpr *expr = new_atom(ctx->pool, fun_lexeme,
                                                     fun_lexeme,
                                                     start + last_split,
                                                     j - last_split);

                            Vec_push(list, expr);
                        }

                        AstExpr *expr = new_atom(ctx->pool, fun_lexeme,
                                                 fun_lexeme, start + j, 1);

                        Vec_push(list, expr);

                        last_split = j + 1;
                    }
                }

                if (last_split < len) {
                    AstExpr *expr = new_atom(ctx->pool, fun_lexeme, fun_lexeme,
                                             start + last_split,
                                             len - last_split);

                    Vec_push(list, expr);
                }
            }
        } else if (toktype == TOK_WORD) {
            AstExpr *expr =
                new_atom(ctx->pool, fun_lexeme, fun_lexeme, start, len);

            Vec_push(list, expr);
        } else {
            // direct token -> expr translation
            AstExpr *expr = new_atom(ctx->pool, fun_literal,
                                     evaltype_of_lit[toktype], start, len);

            Vec_push(list, expr);
        }
    }
}

static void verify_scopes(AstCtx *ctx, const Vec *list) {
    const File *file = ctx->file;

    int level = 0;

    for (size_t i = 0; i < list->len; ++i) {
        AstExpr *expr = list->data[i];

        if (expr->type.id == fun_lexeme.id && expr->tok_len == 1) {
            char ch = file->text.str[expr->tok_start];

            if (ch == '{') {
                ++level;
            } else if (ch == '}') {
                if (--level < 0)
                    AstExpr_error(file, expr, "unmatched curly.");
            }
        }
    }

    if (level > 0)
        File_error_from(file, File_eof(file), "unfinished scope at EOF.");
}

// takes a verified list from gen_flat_list and makes a scope tree
static AstExpr *unflatten_list(AstCtx *ctx, const Vec *list) {
    const RuleTree *rules = &ctx->lang->rules;
    const char *text = ctx->file->text.str;

    Vec levels[MAX_AST_DEPTH];
    size_t level = 0;

    levels[level++] = Vec_new();

    for (size_t i = 0; i < list->len; ++i) {
        AstExpr *expr = list->data[i];

        if (expr->type.id == fun_lexeme.id) {
            if (text[expr->tok_start] == '{') {
                // create new scope vec
                levels[level++] = Vec_new();

                continue;
            } else if (text[expr->tok_start] == '}') {
                // turn scope vec into a scope on pool
                Vec *vec = &levels[--level];

                expr = rule_copy_of_slice(ctx->pool, rules, rules->rule_scope,
                                          (AstExpr **)vec->data, vec->len);

                Vec_del(vec);
            }
        }

        Vec_push(&levels[level - 1], expr);
    }

    AstExpr *tree = rule_copy_of_slice(ctx->pool, rules, rules->rule_scope,
                                       (AstExpr **)levels[0].data,
                                       levels[0].len);

    Vec_del(&levels[0]);

    return tree;
}

static AstExpr *gen_scope_tree(AstCtx *ctx, const TokBuf *tb) {
    Vec root_list = Vec_new();

    gen_flat_list(ctx, &root_list, tb);
    verify_scopes(ctx, &root_list);

    AstExpr *expr = unflatten_list(ctx, &root_list);

    Vec_del(&root_list);

    return expr;
}

// translates `scope` into the target language, returns success
static bool translate_scope(AstCtx *ctx, AstExpr *scope) {
    const Lang *lang = ctx->lang;

    assert(scope->type.id == fun_scope.id);

    AstExpr **exprs = scope->exprs;
    size_t len = scope->len;
    Vec list = Vec_new();

    for (size_t i = 0; i < len; ++i) {
        AstExpr *expr = exprs[i];

        if (expr->type.id == fun_lexeme.id) {
            // symbols must be split up and fully matched, words can be idents
            // or stay lexemes
            View tok = { &ctx->file->text.str[expr->tok_start], expr->tok_len };

            if (ch_is_symbol(tok.str[0])) {
                hsize_t sym_start = expr->tok_start;
                size_t match_len;

                // match as many syms as possible
                while (tok.len > 0
                    && (match_len = HashSet_longest(&lang->syms, &tok))) {
                    AstExpr *sym = new_atom(ctx->pool, fun_lexeme,
                                            fun_lexeme, sym_start, match_len);

                    Vec_push(&list, sym);

                    tok.str += match_len;
                    tok.len -= match_len;
                    sym_start += match_len;
                }

                if (tok.len > 0) {
                    File_error_from(ctx->file, sym_start, "unknown symbol");

                    return false;
                }
            } else {
                Word word = Word_new(tok.str, tok.len);

                if (!HashSet_has(&lang->words, &word)) {
                    expr->type = fun_ident;
                    expr->evaltype = fun_unknown;
                }

                Vec_push(&list, expr);
            }
        } else {
            Vec_push(&list, expr);
        }
    }

    scope->len = list.len;
    scope->exprs = Bump_alloc(ctx->pool, scope->len * sizeof(*scope->exprs));

    for (size_t i = 0; i < list.len; ++i)
        scope->exprs[i] = list.data[i];

    Vec_del(&list);

    return true;
}

// stage 2: rule parsing =======================================================

static bool expr_matches(AstCtx *ctx, const AstExpr *expr,
                         const MatchAtom *pred) {
    bool matches = false;

    if (expr->type.id == fun_lexeme.id) {
        // lexeme
        View token = { &ctx->file->text.str[expr->tok_start], expr->tok_len };

        matches = pred->type == MATCH_LEXEME && Word_eq_view(pred->lxm, &token);
    } else {
        // expr
        matches = pred->type == MATCH_EXPR
               && Type_matches(expr->type, pred->rule_expr);
    }

    return matches;
}

static size_t try_match_r(AstCtx *ctx, const Vec *nexts, AstExpr **slice,
                          size_t len, size_t depth, Rule *o_rule) {
    if (len == 0)
        return 0;

    size_t best_depth = 0;
    Rule rule = {0};

    for (size_t i = 0; i < nexts->len; ++i) {
        const RuleNode *node = nexts->data[i];

        if (expr_matches(ctx, slice[0], node->pred)) {
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
    const PrecGraph *precs = &ctx->lang->precs;

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
                Comparison cmp = Prec_cmp(precs, rule_prec, highest);

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

#if 0
        puts(TC_CYAN "COLLAPSING:" TC_RESET);
        for (size_t j = 0; j < buf_len; ++j)
            AstExpr_dump(buf[j], ctx->lang, ctx->file);

        const Word *prec_name = Prec_name(&ctx->lang->precs, highest);

        printf(TC_YELLOW "matches: (precedence %.*s)" TC_RESET "\n",
               (int)prec_name->len, prec_name->str);
        for (size_t j = 0; j < num_matches; ++j) {
            const MatchSlice *match = &matches[j];
            const Word *name = Rule_get(rules, match->rule)->name;

            printf("%.*s %zu %zu\n", (int)name->len, name->str, match->start,
                   match->len);
        }
#endif

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
    if (!translate_scope(ctx, expr))
        return NULL;

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
#ifdef DEBUG
    double start = time_now();
    size_t start_mem = ctx->pool->total;
#endif

    AstExpr *ast = parse_scope(ctx, gen_scope_tree(ctx, tb));

    if (!ast)
        global_error = true;

#ifdef DEBUG
    if (ast) {
        printf("ast used/total allocated memory: %zu/%zu\n",
               ast_used_memory(ast), ctx->pool->total - start_mem);
    }

    double duration = time_now() - start;

    printf("parsing took %.6fs.\n", duration);
#endif

    return ast;
}
