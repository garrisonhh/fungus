#ifndef FUNGUS_H
#define FUNGUS_H

#include "lang.h"
#include "sema/types.h"
#include "sema/names.h"

// table of (name, enum name, fun name, num supers, super list)
#define BASE_TYPES\
    /* special */\
    X(unknown,    UNKNOWN,    "Unknown",     0, {0}) /* placeholder; invalid */\
    X(any,        ANY,        "Any",         0, {0}) /* truly any valid type */\
    X(any_val,    ANY_VAL,    "AnyValue",    1, { fun_any })\
    X(any_expr,   ANY_EXPR,   "AnyExpr",     1, { fun_any }) /* any AST expr */\
    X(rule,       RULE,       "Rule",        1, { fun_any_expr })\
    X(nil,        NIL,        "nil",         1, { fun_any_val })\
    /* metatypes + parsing */\
    X(scope,      SCOPE,      "Scope",       1, { fun_rule })\
    X(lexeme,     LEXEME,     "Lexeme",      1, { fun_any_expr })\
    X(literal,    LITERAL,    "Literal",     1, { fun_any_expr })\
    X(ident,      IDENT,      "Ident",       1, { fun_any_expr })\
    X(type,       TYPE,       "Type",        1, { fun_any_expr })\
    /* pattern types */\
    X(match,      MATCH,      "Match",       1, { fun_rule })\
    X(match_expr, MATCH_EXPR, "MatchExpr",   1, { fun_rule })\
    X(type_or,    TYPE_OR,    "TypeOr",      1, { fun_rule })\
    X(type_bang,  TYPE_BANG,  "TypeBang",    1, { fun_rule })\
    X(opt_match,  OPT_MATCH,  "OptMatch",    1, { fun_rule })\
    X(rep_match,  REP_MATCH,  "RepMatch",    1, { fun_rule })\
    X(returns,    RETURNS,    "Returns",     1, { fun_rule })\
    X(pattern,    PATTERN,    "Pattern",     1, { fun_rule })\
    X(wh_clause,  WH_CLAUSE,  "WhereClause", 1, { fun_rule })\
    X(where,      WHERE,      "Where",       1, { fun_rule })\
    /* primitives */\
    X(primitive,  PRIMITIVE,  "Primitive",   1, { fun_any_val })\
    X(bool,       BOOL,       "bool",        1, { fun_primitive })\
    X(string,     STRING,     "string",      1, { fun_primitive })\
    X(number,     NUMBER,     "Number",      1, { fun_primitive })\
    X(int,        INT,        "int",         1, { fun_number })\
    X(float,      FLOAT,      "float",       1, { fun_number })

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
    RULE(parens, PARENS, "Parens", "Highest",\
         "`( expr: AnyExpr!T `) -> T where T = Any")\
    RULE(add, ADD, "Add", "AddSub",\
         "lhs: AnyExpr!T `+ rhs: AnyExpr!T -> T where T = int | float")\
    RULE(subtract, SUBTRACT, "Subtract", "AddSub",\
         "lhs: AnyExpr!T `- rhs: AnyExpr!T -> T where T = int | float")\
    RULE(multiply, MULTIPLY, "Multiply", "MulDiv",\
         "lhs: AnyExpr!T `* rhs: AnyExpr!T -> T where T = int | float")\
    RULE(divide, DIVIDE, "Divide", "MulDiv",\
         "lhs: AnyExpr!T `/ rhs: AnyExpr!T -> T where T = int | float")\
    RULE(modulo, MODULO, "Modulo", "MulDiv",\
         "lhs: AnyExpr!T `% rhs: AnyExpr!T -> T where T = int | float")\
    /* variable assignment */\
    RULE(assign, ASSIGN, "Assign",    "Assignment",\
         "name: Ident!T `= value: AnyExpr!T -> T where T = AnyValue")\
    RULE(const_decl, CONST_DECL, "ConstDecl", "Assignment",\
         "`const assign: Assign!AnyValue -> nil")\
    RULE(let_decl, LET_DECL, "LetDecl", "Assignment",\
         "`let assign: Assign!AnyValue -> nil")\
    /* control flow TODO */\
    RULE(if, IF, "If", "Default",\
         "`if cond: AnyExpr!bool body: Scope!T -> T"\
         "    where T = AnyValue")\
    RULE(elif, ELIF, "Elif", "Default",\
         "`elif cond: AnyExpr!bool body: Scope!T -> T where T = AnyValue")\
    RULE(else, ELSE, "Else", "Default",\
         "`else body: Scope!T -> T where T = AnyValue")\
    RULE(if_chain, IF_CHAIN, "IfChain", "Default",\
         "if: If!T elif: Elif!T?* else: Else!T? -> T where T = AnyValue")\

// define fun_X global vars that are defined in fungus_define_base
#define X(NAME, ...) extern Type fun_##NAME;
BASE_TYPES
#undef X
#define RULE(NAME, ...) extern Type fun_##NAME;
BASE_RULES
#undef RULE

#define X(NAME, ENUM_NAME, ...) ID_##ENUM_NAME,
#define RULE(NAME, ENUM_NAME, ...) ID_##ENUM_NAME,
enum BaseTypeIDs { BASE_TYPES BASE_RULES };
#undef RULE
#undef X

extern Lang fungus_lang;

void fungus_define_base(Names *);

void fungus_lang_init(Names *);
void fungus_lang_quit(void);

#endif
