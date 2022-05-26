#include <assert.h>

#include "fungus.h"

Lang fungus_lang;

#define TYPE(NAME, ...) Type fun_##NAME;
BASE_TYPES
#undef TYPE
#define RULE(NAME, ...) Type fun_##NAME;
BASE_RULES
#undef RULE

void fungus_define_base(Names *names) {
#define TYPE(NAME, ENUM_NAME, STR, SUPERS) {\
    Type supers[] = SUPERS;\
    fun_##NAME = Type_define(names, WORD(STR), supers, ARRAY_SIZE(supers));\
}
    BASE_TYPES
#undef TYPE

#define RULE(NAME, ENUM_NAME, STR, ...)\
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

    for (size_t i = 0; i < ARRAY_SIZE(prec_names); ++i)
        Lang_make_prec(&fun, prec_names[i], prec_assocs[i]);

    // rules
#define RULE(...) {0},
    File files[] = { BASE_RULES };
#undef RULE
#define RULE(NAME, ENUM_NAME, STR, PREC, PAT) &fun_##NAME,
    Type *types[] = { BASE_RULES };
#undef RULE
#define RULE(NAME, ENUM_NAME, STR, PREC, PAT) WORD(PREC),
    Word precs[] = { BASE_RULES };
#undef RULE
#define RULE(NAME, ENUM_NAME, STR, PREC, PAT) PAT,
    const char *pats[] = { BASE_RULES };
#undef RULE

    for (size_t i = 0; i < ARRAY_SIZE(pats); ++i) {
        files[i] = pattern_file(pats[i]);

        Prec prec = Prec_by_name(&fun.precs, &precs[i]);

        AstExpr *pre_pat =
            precompile_pattern(&fun.rules.pool, names, &files[i]);

        Lang_legislate(&fun, &files[i], *types[i], prec, pre_pat);
    }

    Lang_crystallize(&fun, names);

    fungus_lang = fun;
}

void fungus_lang_quit(void) {
    Lang_del(&fungus_lang);
}
