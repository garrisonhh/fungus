#include <assert.h>

#include "base_fungus.h"

static Type bin_math_hook(Fungus *fun, Expr *expr) {
    return expr->exprs[0]->ty;
}

void Fungus_define_base(Fungus *fun) {
    /*
     * base types
     */
    TypeDef notype_typedef = { WORD("NoType") };

    fun->t_notype = Type_define(&fun->types, &notype_typedef);

    // runtime
    TypeDef runtype_typedef = { WORD("RunType") };

    fun->t_runtype = Type_define(&fun->types, &runtype_typedef);

    TypeDef string_typedef = { WORD("String"), &fun->t_runtype, 1 };
    TypeDef bool_typedef = { WORD("Bool"), &fun->t_runtype, 1 };
    TypeDef number_typedef = { WORD("Number"), &fun->t_runtype, 1 };

    fun->t_string = Type_define(&fun->types, &string_typedef);
    fun->t_bool = Type_define(&fun->types, &bool_typedef);
    fun->t_number = Type_define(&fun->types, &number_typedef);

    TypeDef int_typedef = { WORD("Int"), &fun->t_number, 1 };
    TypeDef float_typedef = { WORD("Float"), &fun->t_number, 1 };

    fun->t_int = Type_define(&fun->types, &int_typedef);
    fun->t_float = Type_define(&fun->types, &float_typedef);

    // meta
    TypeDef comptype_typedef = { WORD("CompType") };

    fun->t_comptype = Type_define(&fun->types, &comptype_typedef);

    TypeDef literal_typedef = { WORD("Literal"), &fun->t_comptype, 1 };
    TypeDef lexeme_typedef = { WORD("Lexeme"), &fun->t_comptype, 1 };

    fun->t_literal = Type_define(&fun->types, &literal_typedef);
    fun->t_lexeme = Type_define(&fun->types, &lexeme_typedef);

    /*
     * lexemes (Fungus_define_x funcs rely on t_lexeme)
     */
    Type lex_true = Fungus_define_keyword(fun, WORD("true"));
    Type lex_false = Fungus_define_keyword(fun, WORD("false"));

    Type lex_lparen = Fungus_define_symbol(fun, WORD("("));
    Type lex_rparen = Fungus_define_symbol(fun, WORD(")"));
    Type lex_star = Fungus_define_symbol(fun, WORD("*"));
    Type lex_rslash = Fungus_define_symbol(fun, WORD("/"));
    Type lex_percent = Fungus_define_symbol(fun, WORD("%"));
    Type lex_plus = Fungus_define_symbol(fun, WORD("+"));
    Type lex_minus = Fungus_define_symbol(fun, WORD("-"));

    /*
     * precedences
     */
    PrecDef lowest_def = {
        .name = WORD("LowestPrecedence")
    };
    fun->p_lowest = Prec_define(&fun->precedences, &lowest_def);

    Prec addsub_above[] = { fun->p_lowest };
    PrecDef addsub_def = {
        .name = WORD("AddSubPrecedence"),
        .above = addsub_above,
        .above_len = ARRAY_SIZE(addsub_above),
    };
    Prec addsub = Prec_define(&fun->precedences, &addsub_def);

    Prec muldiv_above[] = { addsub };
    PrecDef muldiv_def = {
        .name = WORD("MulDivPrecedence"),
        .above = muldiv_above,
        .above_len = ARRAY_SIZE(muldiv_above),
    };
    Prec muldiv = Prec_define(&fun->precedences, &muldiv_def);

    Prec highest_above[] = { addsub, muldiv };
    PrecDef highest_def = {
        .name = WORD("HighestPrecedence"),
        .above = highest_above,
        .above_len = ARRAY_SIZE(highest_above),
    };
    fun->p_highest = Prec_define(&fun->precedences, &highest_def);

    /*
     * rules
     */
    // boolean literals
#if 0
    Rule_define(fun, &(RuleDef){
        .name = WORD("TrueLiteral"),
        .pat = {
            .pat = (size_t []){ 0 },
            .len = 1,
            .returns = 1,
            .where = (WherePat []){
                { lex_true },
                { fun->t_bool }
            },
            .where_len = 2
        }
    });

    Rule_define(fun, &(RuleDef){
        .name = WORD("FalseLiteral"),
        .pat = {
            .pat = (size_t []){ 0 },
            .len = 1,
            .returns = 1,
            .where = (WherePat []){
                { lex_false },
                { fun->t_bool }
            },
            .where_len = 2
        }
    });

    // parens
    Rule_define(fun, &(RuleDef){
        .name = WORD("Parens"),
        .pat = {
            .pat = (size_t []){ 0, 1, 2 },
            .len = 3,
            .returns = 1,
            .where = (WherePat []){
                { lex_lparen },
                { fun->t_comptype },
                { lex_rparen },
            }
        }
    });
#endif

    // math
    struct {
        Word name;
        Type sym;
    } bin_math_ops[] = {
        { WORD("Add"),      lex_plus    },
        { WORD("Subtract"), lex_minus   },
        { WORD("Multiply"), lex_star    },
        { WORD("Divide"),   lex_rslash  },
        { WORD("Modulo"),   lex_percent },
    };

    for (size_t i = 0; i < ARRAY_SIZE(bin_math_ops); ++i) {
        Rule_define(fun, &(RuleDef){
            .name = bin_math_ops[i].name,
            .pat = {
                .pat = (size_t []){ 0, 1, 0 },
                .len = 3,
                .returns = 0,
                .where = (WherePat []){
                    { fun->t_number },
                    { bin_math_ops[i].sym },
                },
                .where_len = 2
            },
            .hook = bin_math_hook
        });
    }
}

