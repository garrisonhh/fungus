#include <assert.h>

#include "fungus.h"

Lang fungus_lang;
TypeGraph fungus_types;

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
        Prec prec = Prec_by_name(&fun.precs, &prec_name);\
        AstExpr *pre_pat = precompile_pattern(&fun.rules.pool, PAT);\
        Lang_legislate(&fun, WORD(NAME), prec, pre_pat);\
    } while (0);

    RULES
#undef RULE

    fungus_lang = fun;
}

void fungus_lang_quit(void) {
    Lang_del(&fungus_lang);
}

void fungus_types_init(void) {
    TypeGraph types = TypeGraph_new();

#ifdef DEBUG
#define TYPE(A, ...) TY_##A,
    BaseType base_types[] = { BASE_TYPES };
#undef TYPE
#endif

#define TYPE(ENUM, NAME, ABSTRACT, IS_LEN, IS) {\
    .name = WORD(NAME),\
    .type = ABSTRACT ? TTY_ABSTRACT : TTY_CONCRETE,\
    .is = (Type[])IS,\
    .is_len = IS_LEN\
},
    TypeDef base_defs[] = { BASE_TYPES };
#undef TYPE

    for (size_t i = 0; i < TY_COUNT; ++i) {
        Type ty = Type_define(&types, &base_defs[i]);

#ifdef DEBUG
        assert(ty.id == base_types[i]);
#endif
    }

    fungus_types = types;
}

void fungus_types_quit(void) {
    TypeGraph_del(&fungus_types);
}
