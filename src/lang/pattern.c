#include <assert.h>

#include "pattern.h"
#include "../lang.h"
#include "../lex.h"
#include "../parse.h"

Lang pattern_lang;

#define PRECS\
    PREC("Lowest",  LEFT)\
    PREC("Pattern", RIGHT)\
    PREC("Default", LEFT)\
    PREC("Bang",    LEFT)\
    PREC("Or",      LEFT)\
    PREC("Highest", LEFT)

static MatchAtom new_match_expr(Bump *pool, const TypeExpr *rule_expr,
                                const TypeExpr *type_expr) {
    return (MatchAtom){
        .type = MATCH_EXPR,
        .rule_expr = rule_expr,
        .type_expr = type_expr
    };
}

static MatchAtom new_match_lxm(Bump *pool, const char *lxm) {
    Word lxm_word = WORD(lxm);

    return (MatchAtom){
        .type = MATCH_LEXEME,
        .lxm = Word_copy_of(&lxm_word, pool)
    };
}

void pattern_lang_init(void) {
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
        TypeGraph *rtg = &rules->types;
        Bump *p = &rules->pool;

#define GET_PREC(VAR, NAME)\
    Prec VAR;\
    {\
        Word word = WORD(NAME);\
        VAR = Prec_by_name(&lang.precs, &word);\
    }\

        GET_PREC(pattern_prec, "Pattern");
        GET_PREC(default_prec, "Default");
        GET_PREC(or_prec, "Or");
        GET_PREC(bang_prec, "Or");

#undef GET_PREC

#define IMM_TYPE(VAR, NAME) Type VAR = Rule_immediate_type(rules, WORD(NAME))

        IMM_TYPE(ty_match_expr, "MatchExpr");
        IMM_TYPE(ty_or, "TypeOr");
        IMM_TYPE(ty_bang, "TypeBang");
        IMM_TYPE(ty_returns, "Returns");
        IMM_TYPE(ty_pattern, "Pattern");

#undef IMM_TYPE

        const TypeExpr *any_rule_type =
            TypeExpr_sum(p, 2,
                         TypeExpr_atom(p, ty_or),
                         TypeExpr_atom(p, rules->ty_ident));

        MatchAtom *matches;
        size_t len;

        // match expr
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, TypeExpr_atom(p, rules->ty_ident), NULL);
        matches[1] = new_match_lxm(p, ":");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, ty_bang), NULL);

        Lang_immediate_legislate(&lang, ty_match_expr, default_prec,
                                 (Pattern){ .matches = matches, .len = len });

        // type or
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, any_rule_type, NULL);
        matches[1] = new_match_lxm(p, "|");
        matches[2] = new_match_expr(p, any_rule_type, NULL);

        Lang_immediate_legislate(&lang, ty_or, or_prec,
                                 (Pattern){ .matches = matches, .len = len });

        // type sep (bang)
        len = 3;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_expr(p, any_rule_type, NULL);
        matches[1] = new_match_lxm(p, "!");
        matches[2] = new_match_expr(p, TypeExpr_atom(p, rtg->ty_any), NULL);

        Lang_immediate_legislate(&lang, ty_bang, bang_prec,
                                 (Pattern){ .matches = matches, .len = len });

        // return type
        len = 2;
        matches = Bump_alloc(p, len * sizeof(*matches));
        matches[0] = new_match_lxm(p, "->");
        matches[1] = new_match_expr(p, TypeExpr_atom(p, rtg->ty_any), NULL);

        Lang_immediate_legislate(&lang, ty_returns, default_prec,
                                 (Pattern){ .matches = matches, .len = len });

        // pattern
        len = 2;
        matches = Bump_alloc(p, len * sizeof(*matches));

        // TODO match specifically literal lexemes? could be a sema problem
        // exclusively? can I just run sema on patterns?
        const TypeExpr *expr_or_lxm =
            TypeExpr_sum(p, 2,
                         TypeExpr_atom(p, ty_match_expr),
                         TypeExpr_atom(p, rules->ty_literal));

        const TypeExpr *until_returns =
            TypeExpr_sum(p, 2,
                         TypeExpr_atom(p, ty_pattern),
                         TypeExpr_atom(p, ty_returns));

        matches[0] = new_match_expr(p, expr_or_lxm, NULL);
        matches[1] = new_match_expr(p, until_returns, NULL);

        Lang_immediate_legislate(&lang, ty_pattern, pattern_prec,
                                 (Pattern){ .matches = matches, .len = len });
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

bool MatchAtom_equals(const MatchAtom *a, const MatchAtom *b) {
    if (a->type == b->type) {
        switch (a->type) {
        case MATCH_LEXEME:
            return Word_eq(a->lxm, b->lxm);
        case MATCH_EXPR:
            return TypeExpr_equals(a->rule_expr, b->rule_expr);
        }
    }

    return false;
}

// assumes pattern is correct since this is not user-facing
// lol this is a mess and will be replaced.
AstExpr *precompile_pattern(Bump *pool, const char *str) {
    File f = File_from_str("pattern", str, strlen(str));

    // create ast
    TokBuf tokens = lex(&f);
    AstExpr *ast = parse(&(AstCtx){
        .pool = pool,
        .file = &f,
        .lang = &pattern_lang
    }, &tokens);

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

void MatchAtom_print(const MatchAtom *atom, const TypeGraph *rule_types,
                     const TypeGraph *types) {
    switch (atom->type) {
    case MATCH_EXPR: {
        if (atom->rule_expr)
            TypeExpr_print(rule_types, atom->rule_expr);
        else
            printf("_");

        printf("!");

        if (atom->type_expr)
            TypeExpr_print(types, atom->type_expr);
        else
            printf("_");

        break;
    }
    case MATCH_LEXEME: {
        const Word *lxm = atom->lxm;

        printf("`%.*s", (int)lxm->len, lxm->str);

        break;
    }
    }
}

void Pattern_print(const Pattern *pat, const TypeGraph *rule_types,
                   const TypeGraph *types) {
    for (size_t i = 0; i < pat->len; ++i) {
        if (i) printf(" ");
        MatchAtom_print(&pat->matches[i], rule_types, types);
    }
}
