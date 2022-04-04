#include <assert.h>

#include "fungus.h"

#define PRECS\
    PREC("Lowest",     LEFT)\
    PREC("Assignment", RIGHT)\
    PREC("AddSub",     LEFT)\
    PREC("MulDiv",     LEFT)\
    PREC("Highest",    LEFT)

// table of name, prec, pattern
#define RULES\
    RULE("Parens",    "Highest",    "`( expr: Any `)")\
    \
    RULE("Add",       "AddSub",     "a: Number `+ b: Number")\
    RULE("Subtract",  "AddSub",     "a: Number `- b: Number")\
    RULE("Multiply",  "MulDiv",     "a: Number `* b: Number")\
    RULE("Divide",    "MulDiv",     "a: Number `/ b: Number")\
    RULE("Modulo",    "MulDiv",     "a: Number `% b: Number")\
    \
    RULE("Assign",    "Assignment", "name: Ident `= rvalue: Any")\
    RULE("ConstDecl", "Assignment", "`const assign: Assign")\
    RULE("LetDecl",   "Assignment", "`let assign: Assign")\

Lang base_fungus(void) {
#ifdef DEBUG
    // ensure called only once
    static bool called = false;
    assert(!called);
    called = true;
#endif

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

    return fun;
}
