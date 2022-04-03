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

// stage 1: scope tree =========================================================

// TODO get rid of this
#define MAX_SCOPE_STACK 1024

#define X(A) #A,
const char *EX_NAME[EX_COUNT] = { EXPR_TYPES };
#undef X

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
    Expr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (Expr){
        .type = EX_SCOPE,
        .scope = {
            .exprs = exprs,
            .len = len
        }
    };

    return expr;
}

static Expr *scope_from_vec(Bump *pool, Vec *vec) {
    Expr **exprs = Bump_alloc(pool, vec->len * sizeof(*exprs));

    for (size_t j = 0; j < vec->len; ++j)
        exprs[j] = vec->data[j];

    return new_scope(pool, exprs, vec->len);
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
                    Expr_error(file, expr, "unmatched curly.");
            }
        }
    }

    if (level > 0)
        File_error_from(file, File_eof(file), "unfinished scope at EOF.");
}

// takes a verified list from gen_flat_list and makes a scope tree
static Expr *unflatten_list(Bump *pool, const File *file, const Vec *list) {
    const char *text = file->text.str;

    Vec levels[MAX_SCOPE_STACK];
    size_t level = 0;

    levels[level++] = Vec_new();

    for (size_t i = 0; i < list->len; ++i) {
        Expr *expr = list->data[i];

        if (expr->type == EX_LEXEME) {
            if (text[expr->tok_start] == '{') {
                // create new scope vec
                levels[level++] = Vec_new();

                continue;
            } else if (text[expr->tok_start] == '}') {
                // turn scope vec into a scope on pool
                Vec *vec = &levels[--level];

                expr = scope_from_vec(pool, vec);

                Vec_del(vec);
            }
        }

        Vec_push(&levels[level - 1], expr);
    }

    Expr *tree = scope_from_vec(pool, &levels[0]);

    Vec_del(&levels[0]);

    return tree;
}

static Expr *gen_scope_tree(Bump *pool, const TokBuf *tb) {
    Vec root_list = Vec_new();

    gen_flat_list(&root_list, pool, tb);

#if 1
    const char *text = tb->file->text.str;

    puts(TC_CYAN "generated flat list:" TC_RESET);

    for (size_t i = 0; i < root_list.len; ++i) {
        Expr *expr = root_list.data[i];

        printf("%10s `%.*s`\n", EX_NAME[expr->type],
               (int)expr->tok_len, &text[expr->tok_start]);
    }
#endif

    verify_scopes(&root_list, tb->file);

    Expr *expr = unflatten_list(pool, tb->file, &root_list);

    Vec_del(&root_list);

    return expr;
}

// stage 2: rule parsing =======================================================

static void collapse_rules() {

}

Expr *parse_scope(Bump *pool, const File *f, const Lang *lang, Expr *expr) {
    assert(expr->type == EX_SCOPE);

    Expr **slice = expr->scope.exprs;

    for (size_t i = 0; i < expr->scope.len; ++i)
        Expr_error(f, slice[i], "look");

    fungus_panic("TODO this");

    return expr;
}

// general =====================================================================

Expr *parse(Bump *pool, const Lang *lang, const TokBuf *tb) {
    return parse_scope(pool, tb->file, lang, gen_scope_tree(pool, tb));
}

static hsize_t Expr_tok_start(const Expr *expr) {
    while (expr->type == EX_SCOPE)
        expr = expr->scope.exprs[0];

    return expr->tok_start;
}

static hsize_t Expr_tok_len(const Expr *expr) {
    hsize_t start = Expr_tok_start(expr);

    while (expr->type == EX_SCOPE)
        expr = expr->scope.exprs[expr->scope.len - 1];

    return expr->tok_start + expr->tok_len - start;
}

void Expr_error(const File *f, const Expr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_at(f, Expr_tok_start(expr), Expr_tok_len(expr), fmt, args);
    va_end(args);
}

void Expr_error_from(const File *f, const Expr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_from(f, Expr_tok_start(expr), fmt, args);
    va_end(args);
}

void Expr_dump(const Expr *expr, const File *file) {
    const char *text = file->text.str;
    const Expr *scopes[MAX_SCOPE_STACK];
    size_t indices[MAX_SCOPE_STACK];
    size_t size = 0;

    while (true) {
        // print levels TODO
        if (size > 0) {
            for (size_t i = 0; i < size - 1; ++i) {
                const char *c = "│";

                if (indices[i] == scopes[i]->scope.len)
                    c = " ";

                printf("%s   ", c);
            }

            const char *c = indices[size - 1] == scopes[size - 1]->scope.len
                ? "*" : "|";

            printf("%s── ", c);
        }

        printf("%-10s", EX_NAME[expr->type]);

        // print expr
        if (expr->type == EX_SCOPE) {
            scopes[size] = expr;
            indices[size] = 0;
            ++size;
        } else {
            printf(" `%.*s`", (int)expr->tok_len,
                   &text[expr->tok_start]);
        }

        puts("");

        // get next expr
        while (size > 0 && indices[size - 1] >= scopes[size - 1]->scope.len)
            --size;

        if (!size)
            break;

        expr = scopes[size - 1]->scope.exprs[indices[size - 1]++];
    }
}
