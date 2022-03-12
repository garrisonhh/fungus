#include <assert.h>

#include "base_fungus.h"

__attribute__((unused))
static void undefined_hook(Fungus *fun, RuleEntry *entry, Expr *expr) {
    fungus_panic("used undefined rule hook");
}

static void true_hook(Fungus *fun, RuleEntry *entry, Expr *expr) {
    expr->ty = fun->t_bool;
    expr->cty = fun->t_literal;
    expr->bool_ = true;
}

static void false_hook(Fungus *fun, RuleEntry *entry, Expr *expr) {
    expr->ty = fun->t_bool;
    expr->cty = fun->t_literal;
    expr->bool_ = false;
}

static void ternary_hook(Fungus *fun, RuleEntry *entry, Expr *expr) {
    assert(expr->len == 3);

    if (expr->exprs[1]->ty.id != expr->exprs[2]->ty.id) {
        // TODO better error
        fungus_panic("mismatching ternary expr types");
    }

    expr->ty = expr->exprs[1]->ty;
    expr->cty = entry->cty;
}

static void parens_hook(Fungus *fun, RuleEntry *entry, Expr *expr) {
    assert(expr->len == 1);

    expr->ty = expr->exprs[0]->ty;
    expr->cty = entry->cty;
}

static void binary_math_hook(Fungus *fun, RuleEntry *entry, Expr *expr) {
    assert(expr->len == 2);

    if (expr->exprs[0]->ty.id != expr->exprs[1]->ty.id) {
        // TODO better error
        fungus_panic("mismatching binary expr types");
    }

    expr->ty = expr->exprs[0]->ty;
    expr->cty = entry->cty;
}

void Fungus_define_base(Fungus *fun) {
    /*
     * base types
     */
    // runtime
    TypeDef notype_typedef = { WORD("NoType") };
    TypeDef runtype_typedef = { WORD("RunType") };

    fun->t_notype = Type_define(&fun->types, &notype_typedef);
    fun->t_runtype = Type_define(&fun->types, &runtype_typedef);

    Type is_runtype[] = { fun->t_runtype };
    TypeDef string_typedef =
        { WORD("String"), is_runtype, ARRAY_SIZE(is_runtype) };
    TypeDef bool_typedef =
        { WORD("Bool"), is_runtype, ARRAY_SIZE(is_runtype) };
    TypeDef number_typedef =
        { WORD("Number"), is_runtype, ARRAY_SIZE(is_runtype) };

    fun->t_string = Type_define(&fun->types, &string_typedef);
    fun->t_bool = Type_define(&fun->types, &bool_typedef);
    fun->t_number = Type_define(&fun->types, &number_typedef);

    Type is_number[] = { fun->t_number };
    TypeDef int_typedef = { WORD("Int"), is_number, ARRAY_SIZE(is_number) };
    TypeDef float_typedef = { WORD("Float"), is_number, ARRAY_SIZE(is_number) };

    fun->t_int = Type_define(&fun->types, &int_typedef);
    fun->t_float = Type_define(&fun->types, &float_typedef);

    // meta
    TypeDef comptype_typedef = { WORD("CompType") };
    fun->t_comptype = Type_define(&fun->types, &comptype_typedef);

    Type is_comptype[] = { fun->t_comptype };
    TypeDef literal_typedef =
        { WORD("Literal"), is_comptype, ARRAY_SIZE(is_comptype) };
    TypeDef lexeme_typedef =
        { WORD("Lexeme"), is_comptype, ARRAY_SIZE(is_comptype) };
    TypeDef parens_typedef =
        { WORD("Parens"), is_comptype, ARRAY_SIZE(is_comptype) };

    fun->t_literal = Type_define(&fun->types, &literal_typedef);
    fun->t_lexeme = Type_define(&fun->types, &lexeme_typedef);
    fun->t_parens = Type_define(&fun->types, &parens_typedef);

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
    Type lex_dstar = Fungus_define_symbol(fun, WORD("**"));

#define TERNARY_TEST

#ifdef TERNARY_TEST
    Type lex_question = Fungus_define_symbol(fun, WORD("?"));
    Type lex_colon = Fungus_define_symbol(fun, WORD(":"));
#endif

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

    Prec expprec_above[] = { muldiv };
    PrecDef expprec_def = {
        .name = WORD("ExpPrecedence"),
        .above = expprec_above,
        .above_len = ARRAY_SIZE(expprec_above)
    };
    Prec expprec = Prec_define(&fun->precedences, &expprec_def);

    Prec highest_above[] = { addsub, muldiv, expprec };
    PrecDef highest_def = {
        .name = WORD("HighestPrecedence"),
        .above = highest_above,
        .above_len = ARRAY_SIZE(highest_above),
    };
    fun->p_highest = Prec_define(&fun->precedences, &highest_def);

    /*
     * rules
     */
    PatNode true_pat[] = {{ lex_true }};
    RuleDef true_def = {
        .name = WORD("True"),
        .pattern = true_pat,
        .len = ARRAY_SIZE(true_pat),
        .prec = fun->p_highest,
        .hook = true_hook
    };
    Rule_define(fun, &true_def);

    PatNode false_pat[] = {{ lex_false }};
    RuleDef false_def = {
        .name = WORD("False"),
        .pattern = false_pat,
        .len = ARRAY_SIZE(false_pat),
        .prec = fun->p_highest,
        .hook = false_hook
    };
    Rule_define(fun, &false_def);

#ifdef TERNARY_TEST
    TypeDef ternary_typedef =
        { WORD("TernaryExpr"), is_comptype, ARRAY_SIZE(is_comptype) };
    Type ternary = Type_define(&fun->types, &ternary_typedef);

    PatNode ternary_pat[] = {
        { fun->t_bool },
        { lex_question },
        { fun->t_runtype },
        { lex_colon },
        { fun->t_runtype },
    };
    RuleDef ternary_def = {
        .name = WORD("Ternary"),
        .pattern = ternary_pat,
        .len = ARRAY_SIZE(ternary_pat),
        .prec = fun->p_highest,
        .assoc = ASSOC_RIGHT,
        .cty = ternary,
        .hook = ternary_hook
    };
    Rule_define(fun, &ternary_def);
#endif

    PatNode parens_rule_pat[] = {
        { lex_lparen },
        { fun->t_runtype },
        { lex_rparen },
    };
    RuleDef parens_rule_def = {
        .name = WORD("Parens"),
        .pattern = parens_rule_pat,
        .len = ARRAY_SIZE(parens_rule_pat),
        .prec = fun->p_highest,
        .cty = fun->t_parens,
        .hook = parens_hook
    };
    Rule_define(fun, &parens_rule_def);

    // math
    typedef struct BinaryMathOperator {
        Word name;
        Type sym;
        Prec prec;
        Associativity assoc;
    } BinMathOp;

    BinMathOp binary_math_ops[] = {
      /*  name:                 sym:         prec:   */
        { WORD("Multiply"),     lex_star,    muldiv },
        { WORD("Divide"),       lex_rslash,  muldiv },
        { WORD("Modulus"),      lex_percent, muldiv },
        { WORD("Add"),          lex_plus,    addsub },
        { WORD("Subtract"),     lex_minus,   addsub },
        { WORD("Exponentiate"), lex_dstar,   expprec, ASSOC_RIGHT },
    };

    for (size_t i = 0; i < ARRAY_SIZE(binary_math_ops); ++i) {
        BinMathOp *op = &binary_math_ops[i];

        TypeDef op_typedef = { op->name, is_comptype, ARRAY_SIZE(is_comptype) };
        Type op_type = Type_define(&fun->types, &op_typedef);

        PatNode bin_pat[] = {{ fun->t_number }, { op->sym }, { fun->t_number }};
        RuleDef bin_def = {
            .name = op->name,
            .pattern = bin_pat,
            .len = ARRAY_SIZE(bin_pat),
            .prec = op->prec,
            .assoc = op->assoc,
            .cty = op_type,
            .hook = binary_math_hook
        };
        Rule_define(fun, &bin_def);
    }
}

