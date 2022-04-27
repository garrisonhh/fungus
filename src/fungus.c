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
    /* basic math stuff */\
    RULE("Parens",   "Highest",\
         "`( expr: AnyExpr!T `) -> T where T = Any")\
    RULE("Add", "AddSub",\
         "lhs: AnyExpr!T `+ rhs: AnyExpr!T -> T where T = int | float")\
    RULE("Subtract", "AddSub",\
         "lhs: AnyExpr!T `- rhs: AnyExpr!T -> T where T = int | float")\
    RULE("Multiply", "MulDiv",\
         "lhs: AnyExpr!T `* rhs: AnyExpr!T -> T where T = int | float")\
    RULE("Divide", "MulDiv",\
         "lhs: AnyExpr!T `/ rhs: AnyExpr!T -> T where T = int | float")\
    RULE("Modulo", "MulDiv",\
         "lhs: AnyExpr!T `% rhs: AnyExpr!T -> T where T = int | float")\
    /* variable assignment */\
    RULE("Assign",    "Assignment",\
         "name: Ident!T `= value: AnyExpr!T -> T where T = AnyValue")\
    RULE("ConstDecl", "Assignment", "`const assign: Assign!AnyValue -> nil")\
    RULE("LetDecl", "Assignment", "`let assign: Assign!AnyValue -> nil")\

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
#define RULE(...) {0},
    File files[] = { RULES };
#undef RULE
    size_t idx = 0;

    // TODO de-macro-ify this LOL
#define RULE(NAME, PREC, PAT) do {\
        Word prec_name = WORD(PREC);\
        Prec prec = Prec_by_name(&fun.precs, &prec_name);\
        files[idx] = pattern_file(PAT);\
        AstExpr *pre_pat =\
            precompile_pattern(&fun.rules.pool, names, &files[idx]);\
        assert(pre_pat);\
        \
        Word name = WORD(NAME);\
        const NameEntry *entry = name_lookup(names, &name);\
        Type type;\
        if (entry) {\
            assert(entry->type == NAMED_TYPE\
                && entry->type_expr->type == TET_ATOM);\
            type = entry->type_expr->atom;\
        } else {\
            type = Type_define(names, name, &fun_rule, 1);\
        }\
        \
        Lang_legislate(&fun, &files[idx], type, prec, pre_pat);\
        ++idx;\
    } while (0);

    RULES
#undef RULE

    Lang_crystallize(&fun, names);

    fungus_lang = fun;
}

void fungus_lang_quit(void) {
    Lang_del(&fungus_lang);
}
