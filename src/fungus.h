#ifndef FUNGUS_H
#define FUNGUS_H

#include "lang.h"
#include "sema/types.h"
#include "sema/names.h"

// table of (name, fun name, num supers, super list)
#define BASE_TYPES\
    /* special */\
    X(unknown,    "Unknown",     0, {0}) /* placeholder for parsing */\
    X(any,        "Any",         0, {0}) /* truly any valid type */\
    X(any_val,    "AnyValue",    1, { fun_any }) /* any runtime value */\
    X(any_expr,   "AnyExpr",     1, { fun_any }) /* any AST expr */\
    X(rule,       "Rule",        1, { fun_any_expr })\
    X(nil,        "nil",         1, { fun_any_val })\
    /* metatypes + parsing */\
    X(scope,      "Scope",       1, { fun_rule })\
    X(lexeme,     "Lexeme",      1, { fun_any_expr })\
    X(literal,    "Literal",     1, { fun_any_expr })\
    X(ident,      "Ident",       1, { fun_any_expr })\
    X(type,       "Type",        1, { fun_any_expr }) /* fungus type */\
    /* pattern types */\
    X(match,      "Match",       1, { fun_rule }) /* type!evaltype */\
    X(match_expr, "MatchExpr",   1, { fun_rule })\
    X(type_or,    "TypeOr",      1, { fun_rule })\
    X(type_bang,  "TypeBang",    1, { fun_rule })\
    X(opt_match,  "OptMatch",    1, { fun_rule })\
    X(rep_match,  "RepMatch",    1, { fun_rule })\
    X(returns,    "Returns",     1, { fun_rule })\
    X(pattern,    "Pattern",     1, { fun_rule })\
    X(wh_clause,  "WhereClause", 1, { fun_rule })\
    X(where,      "Where",       1, { fun_rule })\
    /* primitives */\
    X(primitive,  "Primitive",   1, { fun_any_val })\
    X(bool,       "bool",        1, { fun_primitive })\
    X(string,     "string",      1, { fun_primitive })\
    X(number,     "Number",      1, { fun_primitive })\
    X(int,        "int",         1, { fun_number })\
    X(float,      "float",       1, { fun_number })

#define BASE_PRECS\
    PREC("Lowest",     LEFT)\
    \
    PREC("Default",    LEFT)\
    PREC("Assignment", RIGHT)\
    PREC("AddSub",     LEFT)\
    PREC("MulDiv",     LEFT)\
    \
    PREC("Highest",    LEFT)

// table of (name, prec, pattern)
#define BASE_RULES\
    /* basic math stuff */\
    RULE(parens, "Parens", "Highest",\
         "`( expr: AnyExpr!T `) -> T where T = Any")\
    RULE(add, "Add", "AddSub",\
         "lhs: AnyExpr!T `+ rhs: AnyExpr!T -> T where T = int | float")\
    RULE(subtract, "Subtract", "AddSub",\
         "lhs: AnyExpr!T `- rhs: AnyExpr!T -> T where T = int | float")\
    RULE(multiply, "Multiply", "MulDiv",\
         "lhs: AnyExpr!T `* rhs: AnyExpr!T -> T where T = int | float")\
    RULE(divide, "Divide", "MulDiv",\
         "lhs: AnyExpr!T `/ rhs: AnyExpr!T -> T where T = int | float")\
    RULE(modulo, "Modulo", "MulDiv",\
         "lhs: AnyExpr!T `% rhs: AnyExpr!T -> T where T = int | float")\
    /* variable assignment */\
    RULE(assign, "Assign",    "Assignment",\
         "name: Ident!T `= value: AnyExpr!T -> T where T = AnyValue")\
    RULE(const_decl, "ConstDecl", "Assignment",\
         "`const assign: Assign!AnyValue -> nil")\
    RULE(let_decl, "LetDecl", "Assignment",\
         "`let assign: Assign!AnyValue -> nil")\
    /* control flow TODO */\
    RULE(if, "If", "Default",\
         "`if cond: AnyExpr!bool body: Scope!T -> T"\
         "    where T = AnyValue")\
    RULE(elif, "Elif", "Default",\
         "`elif cond: AnyExpr!bool body: Scope!T -> T where T = AnyValue")\
    RULE(else, "Else", "Default",\
         "`else body: Scope!T -> T where T = AnyValue")\
    RULE(if_chain, "IfChain", "Default",\
         "if: If!T elif: Elif!T?* else: Else!T? -> T where T = AnyValue")\

// define fun_X global vars that are defined in fungus_define_base
#define X(NAME, ...) extern Type fun_##NAME;
BASE_TYPES
#undef X
#define RULE(NAME, ...) extern Type fun_##NAME;
BASE_RULES
#undef RULE

extern Lang fungus_lang;

void fungus_define_base(Names *);

void fungus_lang_init(Names *);
void fungus_lang_quit(void);

#endif
