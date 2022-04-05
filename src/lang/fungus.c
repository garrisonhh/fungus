#include <assert.h>

#include "fungus.h"

Lang fungus_lang;

#define PRECS\
    PREC("Lowest",     LEFT)\
    PREC("Assignment", RIGHT)\
    PREC("AddSub",     LEFT)\
    PREC("MulDiv",     LEFT)\
    PREC("Highest",    LEFT)

// table of name, prec, pattern
#define RULES\
    RULE("Parens",    "Highest",    "`( expr: T `) -> T")\
    \
    RULE("Add",       "AddSub",     "a: T `+ b: T -> T")\
    RULE("Subtract",  "AddSub",     "a: T `- b: T -> T")\
    RULE("Multiply",  "MulDiv",     "a: T `* b: T -> T")\
    RULE("Divide",    "MulDiv",     "a: T `/ b: T -> T")\
    RULE("Modulo",    "MulDiv",     "a: T `% b: T -> T")\
    \
    // RULE("Assign",    "Assignment", "name: Ident `= rvalue: T -> T")\
    // RULE("ConstDecl", "Assignment", "`const assign: Assign")\
    // RULE("LetDecl",   "Assignment", "`let assign: Assign")

void fungus_lang_init(void) {
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
        Lang_legislate(&fun, &(RuleDef){\
            .name = WORD(NAME),\
            .prec = Prec_by_name(&fun.precs, &prec_name),\
            .pat = Pattern_from(&fun.rules.pool, PAT)\
        });\
    } while (0);

    RULES
#undef RULE

    fungus_lang = fun;
}

void fungus_lang_quit(void) {
    Lang_del(&fungus_lang);
}
