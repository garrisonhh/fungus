#include <assert.h>

#include "parse.h"

/*
 * parsing works in two stages:
 * 1. turn scopes (`{ ... }`) into a tree. this is the only parsing rule that is
 *    truly hardcoded into Fungus
 * 2. recursively parse scopes given a language context. this requires
 *    integration with semantic analysis, because `language`s must be parsed
 *    before they can be used for parsing
 */

// TODO use File_errors throughout this file

#define EXPR_ERROR(FILE_, EXPR, ...)\
    File_error_at(FILE_, (EXPR)->tok_start, (EXPR)->tok_len, __VA_ARGS__)

static Expr *new_expr(Bump *pool, ExprType type, hsize_t start, hsize_t len) {
    Expr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (Expr){
        .type = type,
        .tok_start = start,
        .tok_len = len
    };

    return expr;
}

static Expr *new_scope(Bump *pool, Expr **exprs, size_t len) {
    assert(len > 0);

    Expr *expr = Bump_alloc(pool, sizeof(*expr));

    Expr *leftmost = exprs[0];
    Expr *rightmost = exprs[len - 1];

    *expr = (Expr){
        .type = EX_SCOPE,
        .tok_start = leftmost->tok_start,
        .tok_len =
            (rightmost->tok_start + rightmost->tok_len) - leftmost->tok_start
    };

    return expr;
}

// turns tokens -> list of exprs, separating `{` and `}` as symbols but not
// creating the tree yet
static void gen_flat_list(Vec *list, Bump *pool, const TokBuf *tb) {
    ExprType convert_toktype[TOK_COUNT] = {
        [TOK_INVALID] = EX_INVALID,
        [TOK_WORD] = EX_LEXEME,
        [TOK_BOOL] = EX_LIT_BOOL,
        [TOK_INT] = EX_LIT_INT,
        [TOK_FLOAT] = EX_LIT_FLOAT,
        [TOK_STRING] = EX_LIT_STRING,
    };

    for (size_t i = 0; i < tb->len; ++i) {
        TokType toktype = tb->types[i];

        if (toktype == TOK_SYMBOLS) {
            // split symbols on `{` and `}`
            hsize_t start = tb->starts[i], len = tb->lens[i];
            const char *text = &tb->file->text.str[start];
            hsize_t last_split = 0;

            for (hsize_t i = 0; i < len; ++i) {
                if (text[i] == '{' || text[i] == '}') {
                    if (i > last_split) {
                        Expr *expr = new_expr(pool, EX_LEXEME,
                                              start + last_split,
                                              i - last_split);

                        Vec_push(list, expr);
                    }

                    Vec_push(list, new_expr(pool, EX_LEXEME, start + i, 1));

                    last_split = i + 1;
                }
            }

            if (last_split < len) {
                Expr *expr = new_expr(pool, EX_LEXEME, start + last_split,
                                      len - last_split);

                Vec_push(list, expr);
            }
        } else {
            // direct token -> expr translation
            Expr *expr = new_expr(pool, convert_toktype[toktype], tb->starts[i],
                                  tb->lens[i]);

            Vec_push(list, expr);
        }
    }
}

static void verify_scopes(const Vec *list, const File *file) {
    int level = 0;

    for (size_t i = 0; i < list->len; ++i) {
        Expr *expr = list->data[i];

        if (expr->type == EX_LEXEME && expr->tok_len == 1) {
            char ch = file->text.str[expr->tok_start];

            if (ch == '{') {
                ++level;
            } else if (ch == '}') {
                if (--level < 0)
                    EXPR_ERROR(file, expr, "unmatched curly.");
            }
        }
    }

    if (level > 0)
        File_error_from(file, File_eof(file), "unfinished scope at EOF.");
}

Scope gen_scope_tree(Bump *pool, const TokBuf *tb) {
    Vec root_list = Vec_new();

    gen_flat_list(&root_list, pool, tb);

#if 1
#define X(A) #A,
    const char *exnames[] = { EXPR_TYPES };
#undef X

    const char *text = tb->file->text.str;

    puts(TC_CYAN "generated flat list:" TC_RESET);

    for (size_t i = 0; i < root_list.len; ++i) {
        Expr *expr = root_list.data[i];

        printf("%16s | `%.*s`\n", exnames[expr->type],
               (int)expr->tok_len, &text[expr->tok_start]);
    }
#endif

    verify_scopes(&root_list, tb->file);

    // TODO generate tree from flat list

    exit(0);
}

Expr *parse(Bump *pool, const Lang *lang, const TokBuf *tb) {
    Scope scope = gen_scope_tree(pool, tb);

    // TODO
    return NULL;
}
