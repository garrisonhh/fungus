#include <assert.h>

#include "pattern.h"
#include "ast_expr.h"
#include "../fungus.h"
#include "../lang.h"
#include "../lex.h"
#include "../parse.h"

Lang pattern_lang;

#define PRECS\
    PREC("Lowest",  LEFT)\
    PREC("Pattern", RIGHT)\
    PREC("Default", LEFT)\
    PREC("Match",   LEFT)\
    PREC("Or",      LEFT)\
    PREC("Highest", LEFT)

typedef enum MatchExprFlags {
    REPEATING = 0x1,
    OPTIONAL  = 0x2,
} MatchExprFlags;

static MatchAtom new_match_expr(Bump *pool, const TypeExpr *rule_expr,
                                const TypeExpr *type_expr, unsigned flags) {
    assert(rule_expr && type_expr);

    return (MatchAtom){
        .type = MATCH_EXPR,
        .rule_expr = rule_expr,
        .type_expr = type_expr,
        .repeating = (bool)(flags & REPEATING),
        .optional = (bool)(flags & OPTIONAL),
    };
}

static MatchAtom new_match_lxm(Bump *pool, const char *lxm) {
    Word lxm_word = WORD(lxm);

    return (MatchAtom){
        .type = MATCH_LEXEME,
        .lxm = Word_copy_of(&lxm_word, pool)
    };
}

void pattern_lang_init(Names *names) {
    Lang lang = Lang_new(WORD("PatternLang"));

    // precedences
    {
#define PREC(NAME, ASSOC) WORD(NAME),
        Word prec_names[] = { PRECS };
#undef PREC
#define PREC(NAME, ASSOC) ASSOC_##ASSOC,
        Associativity prec_assocs[] = { PRECS };
#undef PREC

        Prec last;

        for (size_t i = 0; i < ARRAY_SIZE(prec_names); ++i) {
            size_t above_len = 0;
            Prec *above = NULL;

            if (i) {
                above_len = 1;
                above = &last;
            }

            last = Lang_make_prec(&lang, &(PrecDef){
                .name = prec_names[i],
                .assoc = prec_assocs[i],
                .above = above,
                .above_len = above_len
            });
        }
    }

    // rules (pattern lang does not use sema for type checking, you can ignore)
    {
        RuleTree *rules = &lang.rules;
        Bump *p = &rules->pool;

#define GET_PREC(VAR, NAME)\
    Prec VAR;\
    {\
        Word word = WORD(NAME);\
        VAR = Prec_by_name(&lang.precs, &word);\
    }\

        GET_PREC(pattern_prec, "Pattern");
        GET_PREC(default_prec, "Default");
        GET_PREC(or_prec,      "Or");
        GET_PREC(match_prec,   "Match");

#undef GET_PREC

        MatchAtom *matches;
        size_t len;

        // match expr
        const TypeExpr *match_expr_types =
            TypeExpr_sum(p, 3,
                         TypeExpr_atom(p, fun_type_bang),
                         TypeExpr_atom(p, fun_rep_match),
                         TypeExpr_atom(p, fun_opt_match));

        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, fun_ident),
                                    TypeExpr_atom(p, fun_unknown), 0);
        matches[1] = new_match_lxm(p, ":");
        matches[2] = new_match_expr(p, match_expr_types,
                                    TypeExpr_atom(p, fun_match), 0);

        Lang_immediate_legislate(&lang, fun_match_expr, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_match_expr),
        });

        // type or
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);
        matches[1] = new_match_lxm(p, "|");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);

        Lang_immediate_legislate(&lang, fun_type_or, or_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_type),
        });

        // type sep (bang)
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);
        matches[1] = new_match_lxm(p, "!");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);

        Lang_immediate_legislate(&lang, fun_type_bang, match_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_match),
        });

        // optional modifier
        len = 2;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, match_expr_types,
                                    TypeExpr_atom(p, fun_match), 0);
        matches[1] = new_match_lxm(p, "?");

        Lang_immediate_legislate(&lang, fun_opt_match, match_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_match),
        });

        // repeating modifier
        len = 2;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, match_expr_types,
                                    TypeExpr_atom(p, fun_match), 0);
        matches[1] = new_match_lxm(p, "*");

        Lang_immediate_legislate(&lang, fun_rep_match, match_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_match),
        });

        // return type
        len = 2;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_lxm(p, "->");
        matches[1] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);

        Lang_immediate_legislate(&lang, fun_returns, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_returns),
        });

        // where clause
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, fun_ident),
                                    TypeExpr_atom(p, fun_unknown), 0);
        matches[1] = new_match_lxm(p, "=");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);

        Lang_immediate_legislate(&lang, fun_wh_clause, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_wh_clause),
        });

        // where clause series
        len = 2;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_lxm(p, "where");
        matches[1] = new_match_expr(p, TypeExpr_atom(p, fun_wh_clause),
                                    TypeExpr_atom(p, fun_wh_clause),
                                    REPEATING);

        Lang_immediate_legislate(&lang, fun_where, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_where),
        });

        // pattern
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));

        const TypeExpr *expr_or_lxm =
            TypeExpr_sum(p, 2,
                         TypeExpr_atom(p, fun_match_expr),
                         TypeExpr_atom(p, fun_literal));

        const TypeExpr *expr_or_lxm_eval =
            TypeExpr_sum(p, 2,
                         TypeExpr_atom(p, fun_match),
                         TypeExpr_atom(p, fun_lexeme));

        matches[0] = new_match_expr(p, expr_or_lxm, expr_or_lxm_eval,
                                    REPEATING);
        matches[1] = new_match_expr(p, TypeExpr_atom(p, fun_returns),
                                    TypeExpr_atom(p, fun_returns), 0);
        matches[2] = new_match_expr(p, TypeExpr_atom(p, fun_where),
                                    TypeExpr_atom(p, fun_where), OPTIONAL);

        Lang_immediate_legislate(&lang, fun_pattern, pattern_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, fun_pattern),
        });
    }

#ifdef DEBUG
    lang.rules.crystallized = true; // TODO this is ugly
#endif
    pattern_lang = lang;

#ifdef DEBUG
    Lang_dump(&pattern_lang);
#endif
}

void pattern_lang_quit(void) {
    Lang_del(&pattern_lang);
}

bool MatchAtom_matches_rule(const File *file, const MatchAtom *pred,
                            const AstExpr *expr) {
    bool matches = false;

    if (expr->type.id == fun_lexeme.id) {
        // lexeme
        View token = { &file->text.str[expr->tok_start], expr->tok_len };

        matches = pred->type == MATCH_LEXEME && Word_eq_view(pred->lxm, &token);
    } else {
        // expr
        matches = pred->type == MATCH_EXPR
               && Type_matches(expr->type, pred->rule_expr);
    }

    return matches;
}

bool MatchAtom_matches_type(const File *file, const MatchAtom *pred,
                            const AstExpr *expr) {
    if (pred->type == MATCH_LEXEME)
        return true;

    return Type_matches(expr->evaltype, pred->type_expr);
}

// used to determined what RuleNodes can work with each other
bool MatchAtom_equals(const MatchAtom *a, const MatchAtom *b) {
    if (a->type == b->type) {
        switch (a->type) {
        case MATCH_LEXEME:
            return Word_eq(a->lxm, b->lxm);
        case MATCH_EXPR: {
            bool flags_equal = a->repeating == b->repeating
                            && a->optional == b->optional;
            bool ruleexprs_equal = TypeExpr_equals(a->rule_expr, b->rule_expr);
            bool typeexprs_equal = a->type_expr
                ? TypeExpr_equals(a->type_expr, b->type_expr)
                : !b->type_expr;

            return flags_equal && ruleexprs_equal && typeexprs_equal;
        }
        }
    }

    return false;
}

File pattern_file(const char *str) {
    return File_from_str("pattern", str, strlen(str));
}

AstExpr *precompile_pattern(Bump *pool, Names *names, const File *file) {
    // create ast
    TokBuf tokens = lex(file);

    AstExpr *ast = parse(&(AstCtx){
        .pool = pool,
        .file = file,
        .lang = &pattern_lang
    }, &tokens);

    /*
     * used to do sema here but it ended up being problematic, may want to
     * bring it back in the future? unsure
     */

#if 1
    puts(TC_YELLOW "precompiled pattern:" TC_RESET);
    AstExpr_dump(ast, &pattern_lang, file);
#endif

    TokBuf_del(&tokens);

    return ast;
}

static TypeExpr *compile_type_expr(Bump *pool, const Names *names,
                                   const File *file, const AstExpr *expr) {
    if (expr->type.id == fun_ident.id) {
        Word word = AstExpr_as_word(file, expr);
        const NameEntry *entry = name_lookup(names, &word);

#ifdef DEBUG
        if (!entry)
            AstExpr_error(file, expr, "unknown pattern type.");
        else if (entry->type != NAMED_TYPE)
            AstExpr_error(file, expr, "not a pattern type.");
#endif

        return TypeExpr_deepcopy(pool, entry->type_expr);
    } else {
#ifdef DEBUG
        if (expr->type.id != fun_type_or.id)
            AstExpr_error(file, expr, "invalid pattern type expr.");
#endif

        assert(expr->type.id == fun_type_or.id);

        TypeExpr *lhs = compile_type_expr(pool, names, file, expr->exprs[0]);
        TypeExpr *rhs = compile_type_expr(pool, names, file, expr->exprs[2]);

        return TypeExpr_sum(pool, 2, lhs, rhs);
    }
}

static bool expr_is_template(const File *file, const AstExpr *expr,
                             const Word *name) {
    if (expr->type.id != fun_ident.id)
        return false;

    Word ident = AstExpr_as_word(file, expr);

    return Word_eq(&ident, name);
}

// parse `where` clauses, type check patterns, and push template names
// temporarily on Names as types
static WhereClause compile_where_clause(Bump *pool, const Names *names,
                                        const File *file,
                                        const AstExpr *pat,
                                        size_t num_params,
                                        const AstExpr *clause) {
    assert(pat->type.id == fun_pattern.id);
    assert(clause->type.id == fun_wh_clause.id);

    Word name = AstExpr_as_word(file, clause->exprs[0]);

    // figure out constrained param/return types
    size_t con_len = 0, con_cap = 8;
    size_t *constrains = malloc(con_cap * sizeof(*constrains));

    for (size_t i = 0; i < num_params; ++i) {
        const AstExpr *param = pat->exprs[i];

        if (param->type.id != fun_match_expr.id)
            continue;

        const AstExpr *param_match = param->exprs[2];

        while (param_match->type.id != fun_type_bang.id) {
            if (param_match->type.id == fun_opt_match.id)
                param_match = param_match->exprs[0];
            else if (param_match->type.id == fun_rep_match.id)
                param_match = param_match->exprs[0];
            else
                assert(false);
        }

        const AstExpr *param_evaltype = param_match->exprs[2];

        if (expr_is_template(file, param_evaltype, &name)) {
            // found template, add to constrains list
            if (con_len == con_cap) {
                con_cap *= 2;
                constrains =
                    realloc(constrains, con_cap * sizeof(*constrains));
            }

            constrains[con_len++] = i;
        }
    }

    // copy constrains
    size_t *pooled_constrains = Bump_alloc(pool, con_len * sizeof(*constrains));

    for (size_t i = 0; i < con_len; ++i)
        pooled_constrains[i] = constrains[i];

    free(constrains);

    // return type
    bool return_is_template =
        expr_is_template(file, pat->exprs[num_params]->exprs[1], &name);

    return (WhereClause){
        .name = Word_copy_of(&name, pool),
        .type_expr = compile_type_expr(pool, names, file, clause->exprs[2]),
        .constrains = pooled_constrains,
        .num_constrains = con_len,
        .hits_return = return_is_template
    };
}

static void compile_match_atom(MatchAtom *pred, Bump *pool, const Names *names,
                               const File *file, const AstExpr *expr) {
    if (expr->type.id == fun_literal.id) {
        // match lexeme
        assert(expr->evaltype.id == fun_lexeme.id);

        Word word = AstExpr_as_word(file, expr);

        // skip first '`'
        ++word.str;
        --word.len;

        *pred = (MatchAtom){
            .type = MATCH_LEXEME,
            .lxm = Word_copy_of(&word, pool),
        };
    } else {
        // pred expr
        assert(expr->type.id == fun_match_expr.id);

        AstExpr *match = expr->exprs[expr->len - 1];

        bool opt = false, rep = false;

        while (match->type.id != fun_type_bang.id) {
            if (match->type.id == fun_opt_match.id) {
                opt = true;
                match = match->exprs[0];
            } else if (match->type.id == fun_rep_match.id) {
                rep = true;
                match = match->exprs[0];
            } else {
                assert(false);
            }
        }

        assert(match->type.id == fun_type_bang.id);

        *pred = (MatchAtom){
            .type = MATCH_EXPR,
            .rule_expr = compile_type_expr(pool, names, file, match->exprs[0]),
            .type_expr = compile_type_expr(pool, names, file, match->exprs[2]),
            .optional = opt,
            .repeating = rep
        };
    }
}

Pattern compile_pattern(Bump *pool, Names *names, const File *file,
                        const AstExpr *ast) {
    Pattern pat = {0};

    // count number of match atoms
    const AstExpr *pat_expr = ast->exprs[0];

    while (pat.len < pat_expr->len) {
        const AstExpr *expr = pat_expr->exprs[pat.len];

        bool is_match_expr = expr->type.id == fun_match_expr.id;
        bool is_literal_lexeme = expr->type.id == fun_literal.id
                              && expr->evaltype.id == fun_lexeme.id;

        if (!(is_match_expr || is_literal_lexeme))
            break;

        ++pat.len;
    }

    // parse `where` clauses (done first for scoping templated types)
    Names_push_scope(names);

    const AstExpr *where_expr = pat_expr->exprs[pat_expr->len - 1];

    if (where_expr->type.id == fun_where.id) {
        pat.wheres_len = where_expr->len - 1;
        pat.wheres = Bump_alloc(pool, pat.wheres_len * sizeof(*pat.wheres));

        for (size_t i = 1; i < where_expr->len; ++i) {
            WhereClause *clause = &pat.wheres[i - 1];

            *clause =
                compile_where_clause(pool, names, file, pat_expr, pat.len,
                                     where_expr->exprs[i]);

            Names_define_type(names, clause->name, clause->type_expr);
        }
    }

    // parse match atoms
    pat.matches = Bump_alloc(pool, pat.len * sizeof(*pat.matches));

    for (size_t i = 0; i < pat.len; ++i) {
        compile_match_atom(&pat.matches[i], pool, names, file,
                           pat_expr->exprs[i]);
    }

    // parse return value
    const AstExpr *ret_expr = pat_expr->exprs[pat.len];

    assert(ret_expr->type.id == fun_returns.id);

    pat.returns = compile_type_expr(pool, names, file, ret_expr->exprs[1]);

    // drop `where` scope
    Names_drop_scope(names);

#if 0
    printf(TC_CYAN "compiled pattern: " TC_RESET);
    Pattern_print(&pat);
    printf("\n" TC_CYAN "from: " TC_RESET "%.*s\n", (int)file->text.len,
           file->text.str);
#endif

    return pat;
}

void MatchAtom_print(const MatchAtom *atom) {
    switch (atom->type) {
    case MATCH_EXPR:
        if (atom->optional)
            printf("optional ");
        if (atom->repeating)
            printf("repeating ");

        if (atom->rule_expr)
            TypeExpr_print(atom->rule_expr);
        else
            printf(TC_BLUE "_" TC_RESET);

        printf(" ! ");

        if (atom->type_expr)
            TypeExpr_print(atom->type_expr);
        else
            printf(TC_BLUE "_" TC_RESET);

        printf(TC_RESET);

        break;
    case MATCH_LEXEME: {
        const Word *lxm = atom->lxm;

        printf("`" TC_GREEN "%.*s" TC_RESET, (int)lxm->len, lxm->str);

        break;
    }
    }
}

void Pattern_print(const Pattern *pat) {
    for (size_t i = 0; i < pat->len; ++i) {
        if (i) printf(" ");
        MatchAtom_print(&pat->matches[i]);
    }

    printf(" -> ");

    if (pat->returns)
        TypeExpr_print(pat->returns);
    else
        printf(TC_BLUE "_" TC_RESET);
}
