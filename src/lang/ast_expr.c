#include "ast_expr.h"
#include "../lang.h"
#include "../file.h"

static hsize_t AstExpr_tok_start(const AstExpr *expr) {
    while (!expr->is_atom)
        expr = expr->exprs[0];

    return expr->tok_start;
}

static hsize_t AstExpr_tok_len(const AstExpr *expr) {
    hsize_t start = AstExpr_tok_start(expr);

    while (!expr->is_atom)
        expr = expr->exprs[expr->len - 1];

    return expr->tok_start + expr->tok_len - start;
}

void AstExpr_error(const File *f, const AstExpr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_at(f, AstExpr_tok_start(expr), AstExpr_tok_len(expr), fmt, args);
    va_end(args);
}

void AstExpr_error_from(const File *f, const AstExpr *expr, const char *fmt, ...) {
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

            const char *c2 = expr->is_atom
                ? "─" : "┬";

            printf("%s──%s ", c, c2);
        }

        printf(TC_RESET);

        // print name
        if (!expr->is_atom) {
            const Word *name = Rule_get(&lang->rules, expr->rule)->name;

            printf(TC_RED "%.*s" TC_RESET, (int)name->len, name->str);
        }

        // print expr
        if (!expr->is_atom) {
            scopes[size] = expr;
            indices[size] = 0;
            ++size;
        } else if (expr->type.id == fun_lexeme.id
                && expr->atom_type == ATOM_LEXEME) {
            // lexeme
            printf("%.*s", (int)expr->tok_len, &text[expr->tok_start]);
        } else if (expr->type.id == fun_literal.id
                && expr->atom_type == ATOM_LEXEME) {
            // lexeme literal
            printf("`" TC_GREEN "%.*s" TC_RESET,
                   (int)expr->tok_len - 1, &text[expr->tok_start + 1]);
        } else {
            switch (expr->atom_type) {
            case ATOM_IDENT:  printf(TC_BLUE);    break;
            default:          printf(TC_MAGENTA); break;
            }

            printf("%.*s" TC_RESET, (int)expr->tok_len, &text[expr->tok_start]);
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
