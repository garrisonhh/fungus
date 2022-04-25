#include "ast_expr.h"
#include "../lang.h"
#include "../file.h"
#include "../fungus.h"

bool AstExpr_is_atom(const AstExpr *expr) {
    return !Type_is(expr->type, fun_rule);
}

static hsize_t AstExpr_tok_start(const AstExpr *expr) {
    while (!AstExpr_is_atom(expr))
        expr = expr->exprs[0];

    return expr->tok_start;
}

static hsize_t AstExpr_tok_len(const AstExpr *expr) {
    hsize_t start = AstExpr_tok_start(expr);

    while (!AstExpr_is_atom(expr))
        expr = expr->exprs[expr->len - 1];

    return expr->tok_start + expr->tok_len - start;
}

Word AstExpr_as_word(const File *file, const AstExpr *expr) {
    hsize_t start = AstExpr_tok_start(expr);
    hsize_t len = AstExpr_tok_len(expr);

    return Word_new(&file->text.str[start], len);
}

void AstExpr_error(const File *f, const AstExpr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_at(f, AstExpr_tok_start(expr), AstExpr_tok_len(expr), fmt,
                   args);
    va_end(args);
}

void AstExpr_error_from(const File *f, const AstExpr *expr, const char *fmt,
                        ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_from(f, AstExpr_tok_start(expr), fmt, args);
    va_end(args);
}

void AstExpr_dump(const AstExpr *expr, const Lang *lang, const File *file) {
    const char *text = file->text.str;
    const AstExpr *scopes[MAX_AST_DEPTH];
    size_t indices[MAX_AST_DEPTH];
    size_t size = 0;

    while (true) {
        bool is_atom = AstExpr_is_atom(expr);

        // print levels
        printf(TC_DIM);

        if (size > 0) {
            for (size_t i = 0; i < size - 1; ++i) {
                const char *c = "│";

                if (indices[i] == scopes[i]->len)
                    c = " ";

                printf("%s  ", c);
            }

            const char *c = indices[size - 1] == scopes[size - 1]->len
                ? "└" : "├";

            const char *c2 = is_atom ? "─" : "┬";

            printf("%s──%s ", c, c2);
        }

        printf(TC_RESET);

        // print expr
        if (!is_atom) {
            // print rule type
            printf(TC_RED);
            Type_print(expr->type);
            printf(TC_RESET "!" TC_RED);
            Type_print(expr->evaltype);
            printf(TC_RESET " ");

            // move up a scope
            scopes[size] = expr;
            indices[size] = 0;
            ++size;
        } else if (expr->type.id == fun_lexeme.id) {
            // lexeme
            printf("%.*s", (int)expr->tok_len, &text[expr->tok_start]);
        } else if (expr->type.id == fun_literal.id
                && expr->evaltype.id == fun_lexeme.id) {
            // lexeme literal
            printf("`" TC_GREEN "%.*s" TC_RESET,
                   (int)expr->tok_len - 1, &text[expr->tok_start + 1]);
        } else {
            // must be other atom
            if (expr->evaltype.id == fun_ident.id)
                printf(TC_BLUE);
            else
                printf(TC_MAGENTA);

            printf("%.*s" TC_RESET, (int)expr->tok_len,
                   &text[expr->tok_start]);
        }

        puts("");

        // get next expr
        while (size > 0 && indices[size - 1] >= scopes[size - 1]->len)
            --size;

        if (!size)
            break;

        expr = scopes[size - 1]->exprs[indices[size - 1]++];
    }
}
