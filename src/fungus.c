#include <assert.h>

#include "fungus.h"

Lang fungus_lang;

#define X(NAME, ...) Type fun_##NAME;
BASE_TYPES
#undef X

void fungus_define_base(Names *names) {
#define X(NAME, STR, NUM_SUPERS, SUPERS) {\
    Type supers[] = SUPERS;\
    fun_##NAME = Type_define(names, WORD(STR), supers, NUM_SUPERS);\
}
    BASE_TYPES
#undef X
}

#define PRECS\
    PREC("Lowest",     LEFT)\
    PREC("Assignment", RIGHT)\
    PREC("AddSub",     LEFT)\
    PREC("MulDiv",     LEFT)\
    PREC("Highest",    LEFT)

// table of name, prec, pattern
#define RULES\
    RULE("Parens",   "Highest", "`( expr: Any!T `) -> T")\
    \
    RULE("Add",      "AddSub",  "a: Any!T `+ b: Any!T -> T")\
    RULE("Subtract", "AddSub",  "a: Any!T `- b: Any!T -> T")\
    RULE("Multiply", "MulDiv",  "a: Any!T `* b: Any!T -> T")\
    RULE("Divide",   "MulDiv",  "a: Any!T `/ b: Any!T -> T")\
    RULE("Modulo",   "MulDiv",  "a: Any!T `% b: Any!T -> T")\
    \
    // RULE("Assign",    "Assignment", "name: Ident `= rvalue: T -> T")\
    // RULE("ConstDecl", "Assignment", "`const assign: Assign")\
    // RULE("LetDecl",   "Assignment", "`let assign: Assign")

void fungus_lang_init(Names *names) {
    Lang fun = Lang_new(WORD("Fungus"));

    // precedences
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

        last = Lang_make_prec(&fun, &(PrecDef){
            .name = prec_names[i],
            .assoc = prec_assocs[i],
            .above = above,
            .above_len = above_len
        });
    }

    // rules
#define RULE(NAME, PREC, PAT) do {\
        Word prec_name = WORD(PREC);\
        Prec prec = Prec_by_name(&fun.precs, &prec_name);\
        AstExpr *pre_pat = precompile_pattern(&fun.rules.pool, names, PAT);\
        Lang_legislate(&fun, WORD(NAME), prec, pre_pat);\
    } while (0);

    RULES
#undef RULE

    fungus_lang = fun;
}

void fungus_lang_quit(void) {
    Lang_del(&fungus_lang);
}
