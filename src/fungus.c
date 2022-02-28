#include <ctype.h>

#include "fungus.h"
#include "lex.h"

// sorts by descending length
static int lexeme_cmp(const void *a, const void *b) {
    return (int)(*(Word **)b)->len - (int)(*(Word **)a)->len;
}

static Type define_lexeme(Fungus *fun, Word name) {
    TypeDef lexeme_def = { name, &fun->t_lexeme, 1 };

    return Type_define(&fun->types, &lexeme_def);
}

Type Fungus_define_symbol(Fungus *fun, Word symbol) {
    // validate symbol
    if (symbol.len == 0)
        goto invalid_symbol;

    for (size_t i = 0; i < symbol.len; ++i)
        if (!ispunct(symbol.str[i]))
            goto invalid_symbol;

    // register symbol
    Word *copy = Word_copy_of(&symbol, &fun->lexer.pool);

    Vec_push(&fun->lexer.symbols, copy);
    Vec_qsort(&fun->lexer.symbols, lexeme_cmp); // TODO this is inefficient af

    return define_lexeme(fun, symbol);

invalid_symbol:
    fungus_error(">>%.*s<< is not a valid symbol.",
                 (int)symbol.len, symbol.str);
    global_error = true;

    return INVALID_TYPE;
}

Type Fungus_define_keyword(Fungus *fun, Word keyword) {
    // validate keyword
    if (keyword.len == 0 || !isalpha(keyword.str[0]))
        goto invalid_keyword;

    for (size_t i = 1; i < keyword.len; ++i)
        if (!(isalpha(keyword.str[i]) || keyword.str[i] == '_'))
            goto invalid_keyword;

    // register keyword
    Word *copy = Word_copy_of(&keyword, &fun->lexer.pool);

    Vec_push(&fun->lexer.keywords, copy);
    Vec_qsort(&fun->lexer.keywords, lexeme_cmp); // TODO this is inefficient af

    return define_lexeme(fun, keyword);

invalid_keyword:
    fungus_panic(">>%.*s<< is not a valid keyword.",
                 (int)keyword.len, keyword.str);
    global_error = true;

    return INVALID_TYPE;
}

static void Fungus_define_base(Fungus *fun) {
    /*
     * base types
     */
    // runtime
    TypeDef notype_def = { WORD("NoType") };
    TypeDef runtype_def = { WORD("RunType") };

    fun->t_notype = Type_define(&fun->types, &notype_def);
    fun->t_runtype = Type_define(&fun->types, &runtype_def);

    Type is_runtype[] = { fun->t_runtype };
    TypeDef string_def = { WORD("String"), is_runtype, ARRAY_SIZE(is_runtype) };
    TypeDef bool_def = { WORD("Bool"), is_runtype, ARRAY_SIZE(is_runtype) };
    TypeDef number_def = { WORD("Number"), is_runtype, ARRAY_SIZE(is_runtype) };

    fun->t_string = Type_define(&fun->types, &string_def);
    fun->t_bool = Type_define(&fun->types, &bool_def);
    fun->t_number = Type_define(&fun->types, &number_def);

    Type is_number[] = { fun->t_number };
    TypeDef int_def = { WORD("Int"), is_number, ARRAY_SIZE(is_number) };
    TypeDef float_def = { WORD("Float"), is_number, ARRAY_SIZE(is_number) };

    fun->t_int = Type_define(&fun->types, &int_def);
    fun->t_float = Type_define(&fun->types, &float_def);

    // meta
    TypeDef metatype_def = { WORD("MetaType") };
    fun->t_metatype = Type_define(&fun->types, &metatype_def);

    Type is_metatype[] = { fun->t_metatype };
    TypeDef literal_def =
        { WORD("Literal"), is_metatype, ARRAY_SIZE(is_metatype) };
    TypeDef lexeme_def =
        { WORD("Lexeme"), is_metatype, ARRAY_SIZE(is_metatype) };
    TypeDef parens_def =
        { WORD("Parens"), is_metatype, ARRAY_SIZE(is_metatype) };

    fun->t_literal = Type_define(&fun->types, &literal_def);
    fun->t_lexeme = Type_define(&fun->types, &lexeme_def);
    fun->t_parens = Type_define(&fun->types, &parens_def);

    /*
     * lexemes (Fungus_define_x funcs rely on t_lexeme)
     */
    Type lex_true = Fungus_define_keyword(fun, WORD("true"));
    Type lex_false = Fungus_define_keyword(fun, WORD("false"));

    // Type lex_lparen = Fungus_define_symbol(fun, WORD("("));
    // Type lex_rparen = Fungus_define_symbol(fun, WORD(")"));
    Type lex_star = Fungus_define_symbol(fun, WORD("*"));
    Type lex_rslash = Fungus_define_symbol(fun, WORD("/"));
    Type lex_percent = Fungus_define_symbol(fun, WORD("%"));
    Type lex_plus = Fungus_define_symbol(fun, WORD("+"));
    Type lex_minus = Fungus_define_symbol(fun, WORD("-"));

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
    PatNode true_pat[] = {{ lex_true }};
    RuleDef true_def = {
        .pattern = true_pat,
        .len = ARRAY_SIZE(true_pat),
        .prec = fun->p_highest,
        .ty = fun->t_bool,
        .mty = lex_true,
    };
    Rule_define(fun, &true_def);

    PatNode false_pat[] = {{ lex_false }};
    RuleDef false_def = {
        .pattern = false_pat,
        .len = ARRAY_SIZE(false_pat),
        .prec = fun->p_highest,
        .ty = fun->t_bool,
        .mty = lex_false,
    };
    Rule_define(fun, &false_def);

#ifdef TERNARY_TEST
    TypeDef ternary_typedef =
        { WORD("TernaryExpr"), is_metatype, ARRAY_SIZE(is_metatype) };
    Type ternary = Type_define(&fun->types, &ternary_typedef);

    PatNode ternary_pat[] = {
        { fun->t_runtype },
        { lex_question },
        { fun->t_runtype },
        { lex_colon },
        { fun->t_runtype },
    };
    RuleDef ternary_def = {
        .pattern = ternary_pat,
        .len = ARRAY_SIZE(ternary_pat),
        .prec = fun->p_highest,
        .ty = fun->t_runtype,
        .mty = ternary
    };
    Rule_define(fun, &ternary_def);
#endif

    /*
     * PatNode parens_rule_pat[] = {
     *     { lex_lparen },
     *     { fun->t_metatype }, // TODO supertype -> concrete type inference
     *     { lex_rparen },
     * };
     * RuleDef parens_rule_def = {
     *     .pattern = parens_rule_pat,
     *     .len = ARRAY_SIZE(parens_rule_pat),
     *     .prec = fun->p_highest,
     *     .ty = fun->t_runtype, // TODO supertype -> concrete type inference
     *     .mty = fun->t_parens,
     * };
     * Rule_define(fun, &parens_rule_def);
     */

    /*
     * math
     */
    typedef struct BinaryMathOperator {
        Word name;
        Type sym;
        Prec prec;
    } BinMathOp;

    BinMathOp binary_math_ops[] = {
      /*  name:             sym:         prec:   */
        { WORD("Multiply"), lex_star,    muldiv },
        { WORD("Divide"),   lex_rslash,  muldiv },
        { WORD("Modulus"),  lex_percent, muldiv },
        { WORD("Add"),      lex_plus,    addsub },
        { WORD("Subtract"), lex_minus,   addsub },
    };

    for (size_t i = 0; i < ARRAY_SIZE(binary_math_ops); ++i) {
        BinMathOp *op = &binary_math_ops[i];

        // rule metatype
        Type op_is[] = { fun->t_metatype };
        TypeDef op_def = { op->name, op_is, ARRAY_SIZE(op_is) };
        Type op_ty = Type_define(&fun->types, &op_def);

        // rule itself
        PatNode bin_pat[] = {{ fun->t_number }, { op->sym }, { fun->t_number }};
        RuleDef bin_def = {
            .pattern = bin_pat,
            .len = ARRAY_SIZE(bin_pat),
            .prec = op->prec,
            .ty = fun->t_number,
            .mty = op_ty
        };
        Rule_define(fun, &bin_def);
    }
}

Fungus Fungus_new(void) {
    Fungus fun = {
        .lexer = Lexer_new(),
        .types = TypeGraph_new(),
        .precedences = PrecGraph_new(),
        .rules = RuleTree_new(),

        .temp = Bump_new()
    };

    Fungus_define_base(&fun);

#if 1
    Fungus_dump(&fun);
#endif

    return fun;
}

void Fungus_del(Fungus *fun) {
    Bump_del(&fun->temp);

    RuleTree_del(&fun->rules);
    PrecGraph_del(&fun->precedences);
    TypeGraph_del(&fun->types);
    Lexer_del(&fun->lexer);
}

void *Fungus_tmp_alloc(Fungus *fun, size_t n_bytes) {
    return Bump_alloc(&fun->temp, n_bytes);
}

void Fungus_tmp_clear(Fungus *fun) {
    Bump_clear(&fun->temp);
}

void Fungus_dump(Fungus *fun) {
    term_format(TERM_YELLOW);
    puts("Fungus Definitions");
    term_format(TERM_RESET);

    Lexer_dump(&fun->lexer);
    TypeGraph_dump(&fun->types);
    PrecGraph_dump(&fun->precedences);
    RuleTree_dump(fun, &fun->rules);
}

// ensure ty is not a metatype and mty is
static void ensure_valid_types(Fungus *fun, Type ty, Type mty) {
    bool actual_ty = !Type_is(&fun->types, ty, fun->t_metatype);
    bool actual_mty = Type_is(&fun->types, mty, fun->t_metatype);

    if (actual_ty && actual_mty)
        return;

    printf("l%s%sks fishy ><> ", actual_ty ? "o" : "O", actual_mty ? "o" : "O");
}

void Fungus_print_types(Fungus *fun, Type ty, Type mty) {
    ensure_valid_types(fun, ty, mty);

    const Word *ty_name = Type_name(&fun->types, ty);
    const Word *mty_name = Type_name(&fun->types, mty);

    printf("[ %.*s | %.*s ]",
           (int)ty_name->len, ty_name->str,
           (int)mty_name->len, mty_name->str);
}

size_t Fungus_sprint_types(Fungus *fun, char *str, Type ty, Type mty) {
    ensure_valid_types(fun, ty, mty);

    const Word *ty_name = Type_name(&fun->types, ty);
    const Word *mty_name = Type_name(&fun->types, mty);

    return sprintf(str, "[ %.*s | %.*s ]",
                   (int)ty_name->len, ty_name->str,
                   (int)mty_name->len, mty_name->str);
}
