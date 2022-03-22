#include <assert.h>

#include "base_fungus.h"
#include "expr.h"

void Fungus_define_base(Fungus *fun) {
    /*
     * base types
     */
    fun->t_notype = Type_define(&fun->types, &(TypeDef){
        .name = WORD("NoType"),
        .type = TY_ABSTRACT
    });

    // runtime
    fun->t_runtype = Type_define(&fun->types, &(TypeDef){
        .name = WORD("RunType"),
        .type = TY_ABSTRACT
    });
    fun->t_primitive = Type_define(&fun->types, &(TypeDef){
        .name = WORD("Primitive"),
        .type = TY_ABSTRACT,
        .is = &fun->t_runtype,
        .is_len = 1
    });
    fun->t_number = Type_define(&fun->types, &(TypeDef){
        .name = WORD("Number"),
        .type = TY_ABSTRACT,
        .is = &fun->t_primitive,
        .is_len = 1
    });

    fun->t_string = Type_define(&fun->types, &(TypeDef){
        .name = WORD("string"),
        .is = &fun->t_primitive,
        .is_len = 1
    });
    fun->t_bool = Type_define(&fun->types, &(TypeDef){
        .name = WORD("bool"),
        .is = &fun->t_primitive,
        .is_len = 1
    });
    fun->t_int = Type_define(&fun->types, &(TypeDef){
        .name = WORD("int"),
        .is = &fun->t_number,
        .is_len = 1
    });
    fun->t_float = Type_define(&fun->types, &(TypeDef){
        .name = WORD("float"),
        .is = &fun->t_number,
        .is_len = 1
    });

    // comptime-only types
    fun->t_comptype = Type_define(&fun->types, &(TypeDef){
        .name = WORD("CompType"),
        .type = TY_ABSTRACT
    });
    fun->t_lexeme = Type_define(&fun->types, &(TypeDef){
        .name = WORD("Lexeme"),
        .type = TY_ABSTRACT,
        .is = &fun->t_comptype,
        .is_len = 1
    });
    fun->t_syntax = Type_define(&fun->types, &(TypeDef){
        .name = WORD("Syntax"),
        .type = TY_ABSTRACT,
        .is = &fun->t_comptype,
        .is_len = 1
    });

    /*
     * lexemes (Fungus_define_x funcs rely on t_lexeme)
     */
    Type lex_true     = Fungus_base_keyword(fun, WORD("true"));
    Type lex_false    = Fungus_base_keyword(fun, WORD("false"));
    Type lex_lparen   = Fungus_base_symbol(fun,  WORD("("));
    Type lex_rparen   = Fungus_base_symbol(fun,  WORD(")"));
    Type lex_star     = Fungus_base_symbol(fun,  WORD("*"));
    Type lex_rslash   = Fungus_base_symbol(fun,  WORD("/"));
    Type lex_percent  = Fungus_base_symbol(fun,  WORD("%"));
    Type lex_plus     = Fungus_base_symbol(fun,  WORD("+"));
    Type lex_minus    = Fungus_base_symbol(fun,  WORD("-"));
    Type lex_colon    = Fungus_base_symbol(fun,  WORD(":"));
    Type lex_question = Fungus_base_symbol(fun,  WORD("?"));
    Type lex_dstar    = Fungus_base_symbol(fun,  WORD("**"));

    /*
     * precedences
     */
    fun->p_lowest = Prec_define(&fun->precedences, &(PrecDef){
        .name = WORD("LowestPrecedence")
    });
    fun->p_highest = Prec_define(&fun->precedences, &(PrecDef){
        .name = WORD("HighestPrecedence")
    });

    Prec addsub = Prec_define(&fun->precedences, &(PrecDef){
        .name = WORD("AddSubPrecedence"),
        .above = (Prec []){ fun->p_lowest }, .above_len = 1,
        .below = (Prec []){ fun->p_highest }, .below_len = 1
    });
    Prec muldiv = Prec_define(&fun->precedences, &(PrecDef){
        .name = WORD("MulDivPrecedence"),
        .above = (Prec []){ addsub }, .above_len = 1,
        .below = (Prec []){ fun->p_highest }, .below_len = 1
    });
    Prec exp = Prec_define(&fun->precedences, &(PrecDef){
        .name = WORD("ExpPrecedence"),
        .above = (Prec []){ muldiv }, .above_len = 1,
        .below = (Prec []){ fun->p_highest }, .below_len = 1
    });

    /*
     * rules
     */
    Bump _tmp = Bump_new(), *tmp = &_tmp;

    Rule_define(fun, &(RuleDef){
        .name = WORD("TrueLiteral"),
        .pat = {
            .pat = (size_t []){ 0 },
            .len = 1,
            .returns = 1,
            .where = (TypeExpr *[]){
                TypeExpr_atom(tmp, lex_true),
                TypeExpr_atom(tmp, fun->t_bool)
            },
            .where_len = 2
        },
        .prec = fun->p_highest,
    });
    Rule_define(fun, &(RuleDef){
        .name = WORD("FalseLiteral"),
        .pat = {
            .pat = (size_t []){ 0 },
            .len = 1,
            .returns = 1,
            .where = (TypeExpr *[]){
                TypeExpr_atom(tmp, lex_false),
                TypeExpr_atom(tmp, fun->t_bool)
            },
            .where_len = 2
        },
        .prec = fun->p_highest,
    });

    Rule_define(fun, &(RuleDef){
        .name = WORD("Parens"),
        .pat = {
            .pat = (size_t []){ 0, 1, 2 },
            .len = 3,
            .returns = 1,
            .where = (TypeExpr *[]){
                TypeExpr_atom(tmp, lex_lparen),
                TypeExpr_atom(tmp, fun->t_runtype),
                TypeExpr_atom(tmp, lex_rparen)
            },
            .where_len = 3
        },
        .prec = fun->p_highest,
    });

    Rule_define(fun, &(RuleDef){
        .name = WORD("Ternary"),
        .pat = {
            .pat = (size_t []){ 0, 2, 1, 3, 1 },
            .len = 5,
            .returns = 1,
            .where = (TypeExpr *[]){
                TypeExpr_atom(tmp, fun->t_bool),
                TypeExpr_atom(tmp, fun->t_runtype),
                TypeExpr_atom(tmp, lex_question),
                TypeExpr_atom(tmp, lex_colon)
            },
            .where_len = 4
        },
        .prec = fun->p_highest,
        .assoc = ASSOC_RIGHT
    });

    // math
    struct {
        Word name;
        Type sym;
        Prec prec;
        Associativity assoc;
    } bin_math_ops[] = {
        { WORD("Add"),      lex_plus,    addsub },
        { WORD("Subtract"), lex_minus,   addsub },
        { WORD("Multiply"), lex_star,    muldiv },
        { WORD("Divide"),   lex_rslash,  muldiv },
        { WORD("Modulo"),   lex_percent, muldiv },
        { WORD("Power"),    lex_dstar,   exp,   ASSOC_RIGHT },
    };

    for (size_t i = 0; i < ARRAY_SIZE(bin_math_ops); ++i) {
        Rule_define(fun, &(RuleDef){
            .name = bin_math_ops[i].name,
            .pat = {
                .pat = (size_t []){ 0, 1, 0 },
                .len = 3,
                .returns = 0,
                .where = (TypeExpr *[]){
                    TypeExpr_atom(tmp, fun->t_number),
                    TypeExpr_atom(tmp, bin_math_ops[i].sym),
                },
                .where_len = 2
            },
            .prec = bin_math_ops[i].prec,
            .assoc = bin_math_ops[i].assoc,
        });
    }

    Bump_del(tmp);
}

