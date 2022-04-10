#include <assert.h>

#include "parse.h"
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

#define X(A) #A,
const char *ATOM_NAME[ATOM_COUNT] = { ATOM_TYPES };
#undef X

static AstExpr *new_atom(Bump *pool, Type type, AstAtomType atom_type,
                         hsize_t start, hsize_t len) {
    AstExpr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (AstExpr){
        .type = type,
        .is_atom = true,
        .atom_type = atom_type,
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
    const RuleTree *rules = &ctx->lang->rules;

    AstAtomType atomtype_of_lit[TOK_COUNT] = {
        [TOK_BOOL] = ATOM_BOOL,
        [TOK_INT] = ATOM_INT,
        [TOK_FLOAT] = ATOM_FLOAT,
        [TOK_STRING] = ATOM_STRING,
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

                AstExpr *expr = new_atom(ctx->pool, rules->ty_literal,
                                         ATOM_LEXEME, start, len);

                Vec_push(list, expr);
            } else {
                // split symbols on `{` and `}`
                hsize_t last_split = 0;

                for (hsize_t i = 0; i < len; ++i) {
                    if (text[i] == '{' || text[i] == '}') {
                        if (i > last_split) {
                            AstExpr *expr = new_atom(ctx->pool,
                                                     rules->ty_lexeme,
                                                     ATOM_LEXEME,
                                                     start + last_split,
                                                     i - last_split);

                            Vec_push(list, expr);
                        }

                        AstExpr *expr = new_atom(ctx->pool, rules->ty_lexeme,
                                                 ATOM_LEXEME, start + i, 1);

                        Vec_push(list, expr);

                        last_split = i + 1;
                    }
                }

                if (last_split < len) {
                    AstExpr *expr = new_atom(ctx->pool, rules->ty_lexeme,
                                             ATOM_LEXEME, start + last_split,
                                             len - last_split);

                    Vec_push(list, expr);
                }
            }
        } else if (toktype == TOK_WORD) {
            AstExpr *expr =
                new_atom(ctx->pool, rules->ty_lexeme, ATOM_LEXEME, start, len);

            Vec_push(list, expr);
        } else {
            // direct token -> expr translation
            AstExpr *expr = new_atom(ctx->pool, rules->ty_literal,
                                     atomtype_of_lit[toktype], start, len);

            Vec_push(list, expr);
        }
    }
}

static void verify_scopes(AstCtx *ctx, const Vec *list) {
    const RuleTree *rules = &ctx->lang->rules;
    const File *file = ctx->file;

    int level = 0;

    for (size_t i = 0; i < list->len; ++i) {
        AstExpr *expr = list->data[i];

        if (expr->type.id == rules->ty_lexeme.id && expr->tok_len == 1) {
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

        if (expr->type.id == rules->ty_lexeme.id
         && expr->atom_type == ATOM_LEXEME) {
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
    const RuleTree *rules = &lang->rules;

    assert(scope->type.id == rules->ty_scope.id);

    AstExpr **exprs = scope->exprs;
    size_t len = scope->len;
    Vec list = Vec_new();

    for (size_t i = 0; i < len; ++i) {
        AstExpr *expr = exprs[i];

        if (expr->type.id == rules->ty_lexeme.id) {
            // symbols must be split up and fully matched, words can be idents
            // or stay lexemes
            View tok = { &ctx->file->text.str[expr->tok_start], expr->tok_len };

            if (ch_is_symbol(tok.str[0])) {
                hsize_t sym_start = expr->tok_start;
                size_t match_len;

                // match as many syms as possible
                while (tok.len > 0
                    && (match_len = HashSet_longest(&lang->syms, &tok))) {
                    AstExpr *sym = new_atom(ctx->pool, rules->ty_lexeme,
                                            ATOM_LEXEME, sym_start, match_len);

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
                    expr->type = rules->ty_ident;
                    expr->atom_type = ATOM_IDENT;
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
    const RuleTree *rules = &ctx->lang->rules;
    bool matches = false;

    if (expr->type.id == rules->ty_lexeme.id) {
        // lexeme
        View token = { &ctx->file->text.str[expr->tok_start], expr->tok_len };

        matches = pred->type == MATCH_LEXEME && Word_eq_view(pred->lxm, &token);
    } else {
        // expr
        matches = pred->type == MATCH_EXPR
               && Type_matches(&rules->types, expr->type, pred->rule_expr);
    }

    return matches;
}

static size_t try_match_r(AstCtx *ctx, const Vec *nexts, AstExpr **slice,
                          size_t len, Rule *o_rule) {
    if (len == 0)
        return 0;

    size_t best_len = 0;
    Rule rule = {0};

    for (size_t i = 0; i < nexts->len; ++i) {
        const RuleNode *node = nexts->data[i];

        if (expr_matches(ctx, slice[0], node->pred)) {
            // match node children;
            Rule child_rule;
            size_t child_len = try_match_r(ctx, &node->nexts, slice + 1,len - 1,
                                           &child_rule);

            if (child_len > best_len) {
                rule = child_rule;
                best_len = child_len + 1;
            }

            // match node
            if (node->has_rule && best_len < 1) {
                rule = node->rule;
                best_len = 1;
            }
        }
    }

    *o_rule = rule;

    return best_len;
}

// tries to match a rule on a slice, returns length of rule matched
static size_t try_match(AstCtx *ctx, AstExpr **slice, size_t len,
                        Rule *o_rule) {
    return try_match_r(ctx, &ctx->lang->rules.roots, slice, len, o_rule);
}

typedef struct MatchSlice {
    size_t start, len;
    Rule rule;
} MatchSlice;

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
 * rule type checking, etc.
 *
 * in order to avoid as much memory movement as possible, a double-buffered
 * memory situation is used.
 */
static AstExpr **parse_slice(AstCtx *ctx, AstExpr **slice, size_t len,
                             size_t *o_len) {
    const RuleTree *rules = &ctx->lang->rules;
    const PrecGraph *precs = &ctx->lang->precs;

    // construct buffers for slice matching
    AstExpr **buf = malloc(len * sizeof(*slice));
    AstExpr **dbuf = malloc(len * sizeof(*slice));
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

                if (!num_matches || Prec_cmp(precs, rule_prec, highest) > 0)
                    num_matches = 0;

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

        // collapse all matches, re-checking any rules which may have collided
        size_t dbuf_len = 0;

        if (Prec_assoc(precs, highest) == ASSOC_RIGHT) {
            puts(TC_RED "reached right precedence" TC_RESET);
            break;
        } else {
            size_t consumed = 0;

            for (size_t i = 0; i < num_matches; ++i) {
                const MatchSlice *match = &matches[i];

                if (match->start < consumed) {
                    // overlapping match, could be invalidated; must rematch
                    while (consumed < match->start + match->len)
                        dbuf[dbuf_len++] = buf[consumed++];

                    AstExpr **re_slice = &dbuf[dbuf_len - match->len];
                    Rule re_rule;
                    size_t match_len =
                        try_match(ctx, re_slice, match->len, &re_rule);

                    if (match_len && re_rule.id == match->rule.id) {
                        AstExpr *collapsed =
                            rule_copy_of_slice(ctx->pool, rules, match->rule,
                                               re_slice, match->len);

                        dbuf_len -= match->len;
                        dbuf[dbuf_len++] = collapsed;
                    }
                } else {
                    // match without overlap
                    while (consumed < match->start)
                        dbuf[dbuf_len++] = buf[consumed++];

                    dbuf[dbuf_len++] =
                        rule_copy_of_slice(ctx->pool, rules, match->rule,
                                           &buf[match->start], match->len);

                    consumed += match->len;
                }
            }

            while (consumed < buf_len)
                dbuf[dbuf_len++] = buf[consumed++];
        }

        // swap buffers
        AstExpr **swap = buf;

        buf = dbuf;
        dbuf = swap;
        buf_len = dbuf_len;
    }

    // copy parsed buffer to bump-allocated slice
    AstExpr **parsed = Bump_alloc(ctx->pool, buf_len * sizeof(*buf));

    memcpy(parsed, buf, buf_len * sizeof(*parsed));

    // cleanup and return TODO custom arena allocators
    free(matches);
    free(dbuf);
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

    if (!expr->is_atom) {
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
