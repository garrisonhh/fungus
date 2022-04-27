#include <assert.h>

#include "sema.h"
#include "fungus.h"
#include "lang.h"
#include "lang/ast_expr.h"

// expands pattern into `expanded` list
static void expand_pattern(AstExpr *expr, Pattern *pat, MatchAtom *expanded) {
    // TODO
}

/*
 * walks the tree, performing several tasks:
 * - evaluating types for variables + type aliases
 * - typing untyped exprs and checking types for exprs
 *
 * TODO should this also generate variable backrefs?
 *
 * return success
 */
static bool type_check_and_infer(SemaCtx *ctx, AstExpr *expr) {
    Names *names = ctx->names;

    // TODO interpret declarations for var scoping

    if (AstExpr_is_atom(expr)) {
        if (expr->type.id == fun_ident.id) {
            // identify identifier
            Word word =
                Word_new(&ctx->file->text.str[expr->tok_start], expr->tok_len);

            const NameEntry *entry = name_lookup(names, &word);

            if (!entry) {
                // unidentifiable identifer
                expr->evaltype = fun_unknown;
            } else {
                switch (entry->type) {
                case NAMED_TYPE:
                    expr->evaltype = fun_type;
                    break;
                case NAMED_VARIABLE:
                    expr->evaltype = entry->var_type;
                    break;
                default: UNREACHABLE;
                }
            }
        }
    } else {
        // rules
        const RuleEntry *entry = Rule_get(&ctx->lang->rules, expr->rule);

        assert(entry != NULL);

        if (expr->type.id == fun_scope.id) {
            // scopes
            Names_push_scope(names);

            for (size_t i = 0; i < expr->len; ++i)
                if (!type_check_and_infer(ctx, expr->exprs[i]))
                    return false;

            Names_drop_scope(names);

            // evaltype is evaltype of last expr
            if (expr->len == 0)
                expr->evaltype = fun_nil;
            else
                expr->evaltype = expr->exprs[expr->len - 1]->evaltype;
        } else {
            for (size_t i = 0; i < expr->len; ++i)
                if (!type_check_and_infer(ctx, expr->exprs[i]))
                    return false;

            // TODO figure out templated stuff here
            const Pattern *pat = &entry->pat;

            if (pat->returns->type == TET_ATOM)
                expr->evaltype = pat->returns->atom;
        }
    }

    return true;
}

void sema(SemaCtx *ctx, AstExpr *ast) {
    if (!type_check_and_infer(ctx, ast)) {
        global_error = true;
        return;
    }

    // TODO other stuff
}
