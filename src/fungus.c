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
    TypeDef string_def = { WORD("String") };
    TypeDef bool_def = { WORD("Bool") };
    TypeDef number_def = { WORD("Number") };

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

    fun->t_literal = Type_define(&fun->types, &literal_def);
    fun->t_lexeme = Type_define(&fun->types, &lexeme_def);

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

    // TODO math, parens, etc.
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
}
