#include <assert.h>

#include "pattern.h"
#include "../lang.h"
#include "../lex.h"
#include "../parse.h"

Lang pattern_lang;

#define PRECS\
    PREC("Lowest",  LEFT)\
    PREC("Default",  LEFT)\
    PREC("Bang", LEFT)\
    PREC("Or", LEFT)\
    PREC("Highest", LEFT)

static MatchAtom new_match_expr(Bump *pool, const char *name,
                                 const TypeExpr *rule_expr,
                                 const TypeExpr *type_expr) {
    Word name_word = WORD(name);

    return (MatchAtom){
        .type = MATCH_EXPR,
        .name = Word_copy_of(&name_word, pool),
        .rule_expr = rule_expr,
        .type_expr = type_expr
    };
}

static MatchAtom new_match_lxm(Bump *pool, const char *lxm) {
    Word lxm_word = WORD(lxm);

    return (MatchAtom){
        .type = MATCH_LEXEME,
        .name = Word_copy_of(&lxm_word, pool)
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
        MatchAtom *matches;

#define GET_PREC(VAR, NAME)\
    Prec VAR;\
    {\
        Word word = WORD(NAME);\
        VAR = Prec_by_name(&lang.precs, &word);\
    }\

        GET_PREC(default_prec, "Default");
        GET_PREC(bang_prec, "Bang");
        GET_PREC(or_prec, "Or");

#undef GET_PREC

        // match expr
        matches = Bump_alloc(p, 3 * sizeof(*matches));

        matches[0] =
            new_match_expr(p, "ident", TypeExpr_atom(p, rtg->ty_any), NULL);
        matches[1] = new_match_lxm(p, ":");
        matches[2] =
            new_match_expr(p, "type", TypeExpr_atom(p, rtg->ty_any), NULL);

        Lang_immediate_legislate(&lang, WORD("MatchExpr"), default_prec,
                                 (Pattern){ .matches = matches, .len = 3 });

        // type bang
        matches = Bump_alloc(p, 3 * sizeof(*matches));

        matches[0] =
            new_match_expr(p, "lhs", TypeExpr_atom(p, rtg->ty_any), NULL);
        matches[1] = new_match_lxm(p, "!");
        matches[2] =
            new_match_expr(p, "rhs", TypeExpr_atom(p, rtg->ty_any), NULL);

        Lang_immediate_legislate(&lang, WORD("Bang"), bang_prec,
                                 (Pattern){ .matches = matches, .len = 3 });

        // type or
        matches = Bump_alloc(p, 3 * sizeof(*matches));

        matches[0] =
            new_match_expr(p, "lhs", TypeExpr_atom(p, rtg->ty_any), NULL);
        matches[1] = new_match_lxm(p, "|");
        matches[2] =
            new_match_expr(p, "rhs", TypeExpr_atom(p, rtg->ty_any), NULL);

        Lang_immediate_legislate(&lang, WORD("Or"), or_prec,
                                 (Pattern){ .matches = matches, .len = 3 });
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
    AstExpr *ast = parse(pool, &pattern_lang, &tokens);

#if 1
    puts(TC_CYAN "precompiled pattern:" TC_RESET);
    AstExpr_dump(ast, &pattern_lang, &f);
#endif

    TokBuf_del(&tokens);

    return ast;
}

void Pattern_print(const Pattern *pat, const TypeGraph *rule_types,
                   const TypeGraph *types) {
    for (size_t i = 0; i < pat->len; ++i) {
        if (i) printf(" ");

        MatchAtom *atom = &pat->matches[i];

        switch (atom->type) {
        case MATCH_EXPR: {
            const Word *name = atom->name;

            printf("%.*s: ", (int)name->len, name->str);

            if (atom->rule_expr)
                TypeExpr_print(rule_types, atom->rule_expr);

            printf("!");

            if (atom->type_expr)
                TypeExpr_print(types, atom->type_expr);

            break;
        }
        case MATCH_LEXEME: {
            const Word *lxm = atom->lxm;

            printf("`%.*s", (int)lxm->len, lxm->str);

            break;
        }
        }
    }
}
