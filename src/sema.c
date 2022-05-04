#include <assert.h>

#include "sema.h"
#include "fungus.h"
#include "lang.h"
#include "lang/ast_expr.h"

// for pattern checking
static bool check_evaltype(const SemaCtx *ctx, const AstExpr *model,
                           const AstExpr *expr) {
    if (expr->evaltype.id == model->evaltype.id)
        return true;

    const Word *ty_name = Type_name(model->evaltype);

    AstExpr_error(ctx->file, expr,
                  "expected this expr to resolve to type `%.*s`",
                  (int)ty_name->len, ty_name->str);
    AstExpr_error(ctx->file, model, "matching this expression");

    return false;
}

// checks pattern for expr, fills in evaltype, returns success
static bool pattern_check_and_infer(const SemaCtx *ctx, AstExpr *expr,
                                    const Pattern *pat) {
    // store match forms (indices into pattern->matches corresponding to each
    // child)
    size_t *match_forms = malloc(expr->len * sizeof(*match_forms));

    if (pat->len == expr->len) {
        for (size_t i = 0; i < expr->len; ++i)
            match_forms[i] = i;
    } else {
        size_t idx = 0;

        for (size_t i = 0; i < pat->len && idx < expr->len; ++i) {
            const MatchAtom *pred = &pat->matches[i];

            if (pred->repeating) {
                while (idx < expr->len
                    && MatchAtom_matches_rule(ctx->file, pred,
                                              expr->exprs[idx])) {
                    match_forms[idx++] = i;
                }
            } else if (MatchAtom_matches_rule(ctx->file, pred,
                                              expr->exprs[idx])) {
                match_forms[idx++] = i;
            }
        }
    }

    // evaltype checking
    for (size_t i = 0; i < expr->len; ++i) {
        const AstExpr *child = expr->exprs[i];
        const MatchAtom *pred = &pat->matches[match_forms[i]];

        if (!MatchAtom_matches_type(ctx->file, pred, child)) {
            // TODO make this error better, will require some error api work
            AstExpr_error(ctx->file, child, "invalid evaltype");

            printf("got: ");
            Type_print(child->evaltype);
            printf("\nexpected: ");
            TypeExpr_print(pred->type_expr);
            printf("\n");

            return false;
        }
    }

    // where clauses
    bool inferred_ret = false;

    for (size_t i = 0; i < pat->wheres_len; ++i) {
        WhereClause *clause = &pat->wheres[i];

        if (clause->num_constrains) {
            const AstExpr *model = NULL;
            size_t idx = 0;

            for (size_t j = 0;
                 j < clause->num_constrains && idx < expr->len;
                 ++j) {
                size_t constrained = clause->constrains[j];

                // move up to constrained value
                while (match_forms[idx] < constrained && idx < expr->len)
                    ++idx;

                // check constrained value
                while (match_forms[idx] == constrained && idx < expr->len) {
                    if (!model)
                        model = expr->exprs[idx];
                    else if (!check_evaltype(ctx, model, expr->exprs[idx]))
                        return false;

                    ++idx;
                }
            }

            if (clause->hits_return) {
                inferred_ret = true;
                expr->evaltype = model->evaltype;
            }
        }
    }

    free(match_forms);

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

    if (AstExpr_is_atom(expr)) {
        // identify identifiers, all other atoms should have been identified
        // previously
        if (expr->type.id == fun_ident.id) {
            Word word =
                Word_new(&ctx->file->text.str[expr->tok_start], expr->tok_len);

            const NameEntry *entry = name_lookup(names, &word);

            if (!entry) {
                AstExpr_error(ctx->file, expr, "unidentified identifer");

                return false;
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

        assert(expr->evaltype.id != fun_unknown.id);

        return true;
    }

    // rules
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
    } else if (expr->type.id == fun_let_decl.id
            || expr->type.id == fun_const_decl.id) {
        // declarations
        if (!type_check_and_infer(ctx, expr->exprs[1]->exprs[2]))
            return false;

        Word name = AstExpr_as_word(ctx->file, expr->exprs[1]->exprs[0]);
        Type var_type = expr->exprs[1]->exprs[2]->evaltype;

        Names_define_var(names, &name, var_type);

        if (!type_check_and_infer(ctx, expr->exprs[1]))
            return false;

        expr->evaltype = fun_nil;
    } else {
        const RuleEntry *entry = Rule_get(&ctx->lang->rules, expr->rule);

        assert(entry != NULL);

        // normal rules
        for (size_t i = 0; i < expr->len; ++i)
            if (!type_check_and_infer(ctx, expr->exprs[i]))
                return false;

        const Pattern *pat = &entry->pat;

        if (!pattern_check_and_infer(ctx, expr, pat))
            return false;
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
