#include <assert.h>

#include "pattern.h"
#include "../fungus.h"
#include "../lang.h"
#include "../lex.h"
#include "../parse.h"
#include "../sema.h"

Lang pattern_lang;

#define PRECS\
    PREC("Lowest",  LEFT)\
    PREC("Pattern", RIGHT)\
    PREC("Default", LEFT)\
    PREC("Bang",    LEFT)\
    PREC("Or",      LEFT)\
    PREC("Highest", LEFT)

typedef enum MatchExprFlags {
    REPEATING = 0x1,
    OPTIONAL  = 0x2,
} MatchExprFlags;

static MatchAtom new_match_expr(Bump *pool, const TypeExpr *rule_expr,
                                const TypeExpr *type_expr, unsigned flags) {
#ifdef DEBUG
    if (!rule_expr || !type_expr)
        fungus_panic("wuh oh, invalid match_expr");
#endif

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
        GET_PREC(bang_prec,    "Bang");

#undef GET_PREC

#define IMM_TYPE(VAR, NAME) Type VAR = Rule_immediate_type(names, WORD(NAME))

        IMM_TYPE(ty_match_expr, "MatchExpr");
        IMM_TYPE(ty_or,         "TypeOr");
        IMM_TYPE(ty_bang,       "TypeBang");
        IMM_TYPE(ty_returns,    "Returns");
        IMM_TYPE(ty_pattern,    "Pattern");
        IMM_TYPE(ty_wh_clause,  "WhereClause");
        IMM_TYPE(ty_where,      "Where");

#undef IMM_TYPE

        MatchAtom *matches;
        size_t len;

        // match expr
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, fun_ident),
                                    TypeExpr_atom(p, fun_ident), 0);
        matches[1] = new_match_lxm(p, ":");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, ty_bang),
                                    TypeExpr_atom(p, fun_match), 0);

        Lang_immediate_legislate(&lang, ty_match_expr, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, ty_match_expr),
        });

        // type or
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);
        matches[1] = new_match_lxm(p, "|");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);

        Lang_immediate_legislate(&lang, ty_or, or_prec, (Pattern){
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

        Lang_immediate_legislate(&lang, ty_bang, bang_prec, (Pattern){
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

        Lang_immediate_legislate(&lang, ty_returns, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, ty_returns),
        });

        // where clause
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, fun_ident),
                                    TypeExpr_atom(p, fun_unknown), 0);
        matches[1] = new_match_lxm(p, "is");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, fun_any_expr),
                                    TypeExpr_atom(p, fun_type), 0);

        Lang_immediate_legislate(&lang, ty_wh_clause, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, ty_wh_clause),
        });

        // where clause series
        len = 2;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_lxm(p, "where");
        matches[1] = new_match_expr(p, TypeExpr_atom(p, ty_wh_clause),
                                    TypeExpr_atom(p, ty_wh_clause),
                                    REPEATING);

        Lang_immediate_legislate(&lang, ty_where, default_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, ty_where),
        });

        // pattern
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));

        const TypeExpr *expr_or_lxm =
            TypeExpr_sum(p, 2,
                         TypeExpr_atom(p, ty_match_expr),
                         TypeExpr_atom(p, fun_literal));

        const TypeExpr *expr_or_lxm_eval =
            TypeExpr_sum(p, 2,
                         TypeExpr_atom(p, fun_match),
                         TypeExpr_atom(p, fun_lexeme));

        matches[0] = new_match_expr(p, expr_or_lxm, expr_or_lxm_eval,
                                    REPEATING);
        matches[1] = new_match_expr(p, TypeExpr_atom(p, ty_returns),
                                    TypeExpr_atom(p, ty_returns), 0);
        matches[2] = new_match_expr(p, TypeExpr_atom(p, ty_where),
                                    TypeExpr_atom(p, ty_where), OPTIONAL);

        Lang_immediate_legislate(&lang, ty_pattern, pattern_prec, (Pattern){
            .matches = matches,
            .len = len,
            .returns = TypeExpr_atom(p, ty_pattern),
        });
    }

    RuleTree_crystallize(&lang.rules);

    pattern_lang = lang;

#ifdef DEBUG
    Lang_dump(&pattern_lang);
#endif
}

void pattern_lang_quit(void) {
    Lang_del(&pattern_lang);
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

// for internal use only.
AstExpr *precompile_pattern(Bump *pool, Names *names, const char *str) {
    File f = File_from_str("pattern", str, strlen(str));

    // create ast
    TokBuf tokens = lex(&f);
    AstExpr *ast = parse(&(AstCtx){
        .pool = pool,
        .file = &f,
        .lang = &pattern_lang
    }, &tokens);

    sema(&(SemaCtx){
        .pool = pool,
        .file = &f,
        .lang = &pattern_lang,
        .names = names
    }, ast);

#if 1
    puts(TC_CYAN "precompiled pattern:" TC_RESET);
    AstExpr_dump(ast, &pattern_lang, &f);
#endif

    TokBuf_del(&tokens);

    return ast;
}

Pattern compile_pattern(Bump *pool, const Lang *lang, AstExpr *ast) {
    UNIMPLEMENTED;
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
        if (i) printf(TC_DIM " -> " TC_RESET);
        MatchAtom_print(&pat->matches[i]);
    }
}
