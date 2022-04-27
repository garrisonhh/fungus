#include <assert.h>

#include "fungus.h"

Lang fungus_lang;

#define X(NAME, ...) Type fun_##NAME;
BASE_TYPES
#undef X
#define RULE(NAME, ...) Type fun_##NAME;
BASE_RULES
#undef RULE

void fungus_define_base(Names *names) {
#define X(NAME, STR, NUM_SUPERS, SUPERS) {\
    Type supers[] = SUPERS;\
    fun_##NAME = Type_define(names, WORD(STR), supers, NUM_SUPERS);\
}
    BASE_TYPES
#undef X

#define RULE(NAME, STR, ...)\
    fun_##NAME = Type_define(names, WORD(STR), &fun_rule, 1);

    BASE_RULES
#undef RULE
}

void fungus_lang_init(Names *names) {
    Lang fun = Lang_new(WORD("Fungus"));

    // precedences
#define PREC(NAME, ASSOC) WORD(NAME),
    Word prec_names[] = { BASE_PRECS };
#undef PREC
#define PREC(NAME, ASSOC) ASSOC_##ASSOC,
    Associativity prec_assocs[] = { BASE_PRECS };
#undef PREC

    Prec last;

    for (size_t i = 0; i < ARRAY_SIZE(prec_names); ++i) {
        size_t above_len = 0;
        Prec *above = NULL;

        if (i) {
            above_len = 1;
            above = &last;
        }

        last = Lang_make_prec(&fun, &(PrecDef){
            .name = prec_names[i],
            .assoc = prec_assocs[i],
            .above = above,
            .above_len = above_len
        });
    }

    // rules
#define RULE(...) {0},
    File files[] = { BASE_RULES };
#undef RULE
#define RULE(NAME, STR, PREC, PAT) &fun_##NAME,
    Type *types[] = { BASE_RULES };
#undef RULE
#define RULE(NAME, STR, PREC, PAT) WORD(PREC),
    Word precs[] = { BASE_RULES };
#undef RULE
#define RULE(NAME, STR, PREC, PAT) PAT,
    const char *pats[] = { BASE_RULES };
#undef RULE

    for (size_t i = 0; i < ARRAY_SIZE(pats); ++i) {
        files[i] = pattern_file(pats[i]);

        Prec prec = Prec_by_name(&fun.precs, &precs[i]);
        AstExpr *pre_pat =
            precompile_pattern(&fun.rules.pool, names, &files[i]);

        Lang_legislate(&fun, &files[i], *types[i], prec, pre_pat);
    }

    /*
    // TODO de-macro-ify this LOL
#define RULE(NAME, STR, PREC, PAT) do {\
        Word prec_name = WORD(PREC);\
        Prec prec = Prec_by_name(&fun.precs, &prec_name);\
        files[idx] = pattern_file(PAT);\
        AstExpr *pre_pat =\
            precompile_pattern(&fun.rules.pool, names, &files[idx]);\
        assert(pre_pat);\
        \
        Word name = WORD(STR);\
        const NameEntry *entry = name_lookup(names, &name);\
        assert(entry->type == NAMED_TYPE\
            && entry->type_expr->type == TET_ATOM);\
        \
        Lang_legislate(&fun, &files[idx], entry->type_expr->atom, prec,\
                       pre_pat);\
        ++idx;\
    } while (0);

    BASE_RULES
#undef RULE
    */

    Lang_crystallize(&fun, names);

    fungus_lang = fun;
}

void fungus_lang_quit(void) {
    Lang_del(&fungus_lang);
}
