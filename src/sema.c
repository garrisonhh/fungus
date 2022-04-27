#include <assert.h>

#include "sema.h"
#include "fungus.h"
#include "lang.h"
#include "lang/ast_expr.h"

// for pattern checking
static bool check_evaltype(const SemaCtx *ctx, const AstExpr *model,
                           const AstExpr *expr, Type ty) {
    if (expr->evaltype.id == ty.id)
        return true;

    const Word *ty_name = Type_name(ty);

    AstExpr_error(ctx->file, expr,
                  "expected this expr to resolve to type `%.*s`",
                  (int)ty_name->len, ty_name->str);
    AstExpr_error(ctx->file, model, "matching this expression");

    return false;
}

// checks pattern for expr, fills in evaltype, returns success
static bool pattern_check_and_infer(const SemaCtx *ctx, AstExpr *expr,
                                    const Pattern *pat) {
    // pattern expansion TODO
    if (pat->len != expr->len && pat->wheres_len)
        UNIMPLEMENTED;

    // type inference
    bool inferred_ret = false;

    // where clauses
    for (size_t i = 0; i < pat->wheres_len; ++i) {
        WhereClause *clause = &pat->wheres[i];
        Type resolved = fun_unknown;

        if (clause->num_constrains) {
            const AstExpr *model = expr->exprs[clause->constrains[0]];

            resolved = model->evaltype;

            for (size_t j = 1; j < clause->num_constrains; ++j) {
                size_t idx = clause->constrains[j];

                if (!check_evaltype(ctx, model, expr->exprs[idx], resolved))
                    return false;
            }
        }

        if (clause->hits_return) {
            inferred_ret = true;
            expr->evaltype = resolved;
        }
    }

    // evaltype is uninferred
    if (!inferred_ret) {
        assert(pat->returns->type == TET_ATOM);

        expr->evaltype = pat->returns->atom;
    }

    return true;
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
            // normal rules
            for (size_t i = 0; i < expr->len; ++i)
                if (!type_check_and_infer(ctx, expr->exprs[i]))
                    return false;

            const Pattern *pat = &entry->pat;

            if (!pattern_check_and_infer(ctx, expr, pat))
                return false;
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
