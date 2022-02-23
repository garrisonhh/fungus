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
    TypeDef anytype_def = { WORD("AnyType") };
    TypeDef string_def = { WORD("String") };
    TypeDef bool_def = { WORD("Bool") };
    TypeDef number_def = { WORD("Number") };

    fun->t_notype = Type_define(&fun->types, &notype_def);
    fun->t_anytype = Type_define(&fun->types, &anytype_def);
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
    TypeDef anymetatype_def =
        { WORD("AnyMetaType"), is_metatype, ARRAY_SIZE(is_metatype) };
    TypeDef literal_def =
        { WORD("Literal"), is_metatype, ARRAY_SIZE(is_metatype) };
    TypeDef lexeme_def =
        { WORD("Lexeme"), is_metatype, ARRAY_SIZE(is_metatype) };
    TypeDef parens_def =
        { WORD("Parens"), is_metatype, ARRAY_SIZE(is_metatype) };

    fun->t_anymetatype = Type_define(&fun->types, &anymetatype_def);
    fun->t_literal = Type_define(&fun->types, &literal_def);
    fun->t_lexeme = Type_define(&fun->types, &lexeme_def);
    fun->t_parens = Type_define(&fun->types, &parens_def);

    // TODO basic type literals

    /*
     * lexemes (rely on t_lexeme)
     */
    Type t_true_literal = Fungus_define_keyword(fun, WORD("true"));
    Type t_false_literal = Fungus_define_keyword(fun, WORD("false"));
    Type lparen = Fungus_define_symbol(fun, WORD("("));
    Type rparen = Fungus_define_symbol(fun, WORD(")"));

    /*
     * precedences
     */
    PrecDef lowest_def = {
        .name = WORD("LowestPrecedence")
    };
    Prec lowest = Prec_define(&fun->precedences, &lowest_def);

    Prec addsub_above[] = { lowest };
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
    Prec highest = Prec_define(&fun->precedences, &highest_def);

    /*
     * rules
     */
    RuleAtom true_pat[] = {{ t_true_literal }};
    RuleDef true_def = {
        .pattern = true_pat,
        .len = ARRAY_SIZE(true_pat),
        .prec = highest,
        .ty = fun->t_bool,
        .mty = t_true_literal,
    };
    Rule_define(&fun->rules, &true_def);

    RuleAtom false_pat[] = {{ t_false_literal }};
    RuleDef false_def = {
        .pattern = false_pat,
        .len = ARRAY_SIZE(false_pat),
        .prec = highest,
        .ty = fun->t_bool,
        .mty = t_false_literal,
    };
    Rule_define(&fun->rules, &false_def);

    RuleAtom parens_rule_pat[] = {
        { lparen },
        { fun->t_anymetatype },
        { rparen },
    };
    RuleDef parens_rule_def = {
        .pattern = parens_rule_pat,
        .len = ARRAY_SIZE(parens_rule_pat),
        .prec = highest,
        .ty = fun->t_anytype,
        .mty = fun->t_parens,
    };
    Rule_define(&fun->rules, &parens_rule_def);
}

Fungus Fungus_new(void) {
    Fungus fun = {
        .lexer = Lexer_new(),
        .types = TypeGraph_new(),
        .precedences = PrecGraph_new(),
        .rules = RuleTree_new()
    };

    Fungus_define_base(&fun);

#if 1
    Fungus_dump(&fun);
#endif

    return fun;
}

void Fungus_del(Fungus *fun) {
    RuleTree_del(&fun->rules);
    PrecGraph_del(&fun->precedences);
    TypeGraph_del(&fun->types);
    Lexer_del(&fun->lexer);
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
    if (!Type_is(&fun->types, ty, fun->t_metatype)
        && Type_is(&fun->types, mty, fun->t_metatype)) {
        return;
    }

    fungus_panic("received invalid type pair to a Fungus_print_types func");
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
