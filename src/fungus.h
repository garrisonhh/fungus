#ifndef FUNGUS_H
#define FUNGUS_H

#include "lang.h"
#include "sema/types.h"
#include "sema/names.h"

// table of (name, enum name, fun name, list of super types)
#define BASE_TYPES\
    /* special */\
    TYPE(unknown,    UNKNOWN,    "Unknown",     (Type[0]){}) /* placeholder */\
    /* placeholder to encode when a scope is raw for ast parsing */\
    TYPE(raw_scope,  RAW_SCOPE,  "RawScope",    (Type[0]){})\
    TYPE(any,        ANY,        "Any",         (Type[0]){})\
    TYPE(any_val,    ANY_VAL,    "AnyValue",    { fun_any })\
    TYPE(any_expr,   ANY_EXPR,   "AnyExpr",     { fun_any })\
    TYPE(rule,       RULE,       "Rule",        { fun_any_expr })\
    TYPE(nil,        NIL,        "nil",         { fun_any_val })\
    /* metatypes + parsing */\
    TYPE(scope,      SCOPE,      "Scope",       { fun_rule })\
    TYPE(lexeme,     LEXEME,     "Lexeme",      { fun_any_expr })\
    TYPE(literal,    LITERAL,    "Literal",     { fun_any_expr })\
    TYPE(ident,      IDENT,      "Ident",       { fun_any_expr })\
    TYPE(type,       TYPE,       "Type",        { fun_any_expr })\
    /* pattern types */\
    TYPE(match,      MATCH,      "Match",       { fun_rule })\
    TYPE(match_expr, MATCH_EXPR, "MatchExpr",   { fun_rule })\
    TYPE(type_or,    TYPE_OR,    "TypeOr",      { fun_rule })\
    TYPE(type_bang,  TYPE_BANG,  "TypeBang",    { fun_rule })\
    TYPE(opt_match,  OPT_MATCH,  "OptMatch",    { fun_rule })\
    TYPE(rep_match,  REP_MATCH,  "RepMatch",    { fun_rule })\
    TYPE(returns,    RETURNS,    "Returns",     { fun_rule })\
    TYPE(pattern,    PATTERN,    "Pattern",     { fun_rule })\
    TYPE(wh_clause,  WH_CLAUSE,  "WhereClause", { fun_rule })\
    TYPE(where,      WHERE,      "Where",       { fun_rule })\
    /* primitives */\
    TYPE(primitive,  PRIMITIVE,  "Primitive",   { fun_any_val })\
    TYPE(bool,       BOOL,       "bool",        { fun_primitive })\
    /* TODO string probably should be a struct type */\
    TYPE(string,     STRING,     "string",      { fun_primitive })\
    TYPE(number,     NUMBER,     "Number",      { fun_primitive })\
    TYPE(int,        INT,        "int",         { fun_number })\
    TYPE(float,      FLOAT,      "float",       { fun_number })

#define BASE_PRECS\
    /* 'default' is the precedence of language constructs */\
    PREC("Default",      LEFT)\
    PREC("FieldAccess",  LEFT)\
    PREC("Assignment",   RIGHT)\
    PREC("Declaration",  LEFT)\
    PREC("LogicalOr",    LEFT)\
    PREC("LogicalAnd",   LEFT)\
    PREC("Conditional",  LEFT)\
    PREC("Bitshift",     LEFT)\
    PREC("BitwiseOr",    LEFT)\
    PREC("BitwiseAnd",   LEFT)\
    PREC("AddSub",       LEFT)\
    PREC("MulDiv",       LEFT)\
    PREC("UnaryPrefix",  RIGHT)\
    PREC("UnaryPostfix", LEFT)\

// table of (name, enum name, fun name, prec, pattern)
#define BASE_RULES\
    RULE(parens, PARENS, "Parens", "Default",\
         "`( expr: AnyExpr!T `) -> T where T = AnyValue")\
    /* conditional operators */\
    RULE(eq, EQ, "Equals", "Conditional",\
         "lhs: AnyExpr!T `== rhs: AnyExpr!T -> bool where T = Primitive")\
    RULE(ne, NE, "NotEquals", "Conditional",\
         "lhs: AnyExpr!T `!= rhs: AnyExpr!T -> bool where T = Primitive")\
    RULE(lt, LT, "LessThan", "Conditional",\
         "lhs: AnyExpr!T `< rhs: AnyExpr!T -> bool where T = Number")\
    RULE(gt, GT, "GreaterThan", "Conditional",\
         "lhs: AnyExpr!T `> rhs: AnyExpr!T -> bool where T = Number")\
    RULE(le, LE, "LessThanOrEquals", "Conditional",\
         "lhs: AnyExpr!T `<= rhs: AnyExpr!T -> bool where T = Number")\
    RULE(ge, GE, "GreaterThanOrEquals", "Conditional",\
         "lhs: AnyExpr!T `>= rhs: AnyExpr!T -> bool where T = Number")\
    /* logical operators */\
    RULE(or, OR, "Or", "LogicalOr",\
         "lhs: AnyExpr!bool `or rhs: AnyExpr!bool -> bool")\
    RULE(and, AND, "And", "LogicalAnd",\
         "lhs: AnyExpr!bool `and rhs: AnyExpr!bool -> bool")\
    RULE(not, NOT, "Not", "UnaryPrefix",\
         "`! expr: AnyExpr!bool -> bool")\
    /* math operators */\
    RULE(add, ADD, "Add", "AddSub",\
         "lhs: AnyExpr!T `+ rhs: AnyExpr!T -> T where T = Number")\
    RULE(sub, SUB, "Subtract", "AddSub",\
         "lhs: AnyExpr!T `- rhs: AnyExpr!T -> T where T = Number")\
    RULE(mul, MUL, "Multiply", "MulDiv",\
         "lhs: AnyExpr!T `* rhs: AnyExpr!T -> T where T = Number")\
    RULE(div, DIV, "Divide", "MulDiv",\
         "lhs: AnyExpr!T `/ rhs: AnyExpr!T -> T where T = Number")\
    RULE(mod, MOD, "Modulo", "MulDiv",\
         "lhs: AnyExpr!T `% rhs: AnyExpr!T -> T where T = Number")\
    /* assignment */\
    RULE(assign, ASSIGN, "Assign", "Assignment",\
         "name: Ident!T `= value: AnyExpr!T -> T where T = AnyValue")\
    RULE(const_decl, CONST_DECL, "ConstDecl", "Declaration",\
         "`const name: Ident!Unknown|Any `= value: AnyExpr!AnyValue -> nil")\
    RULE(val_decl, VAL_DECL, "ValDecl", "Declaration",\
         "`val name: Ident!Unknown|Any `= value: AnyExpr!AnyValue -> nil")\
    RULE(let_decl, LET_DECL, "LetDecl", "Declaration",\
         "`let name: Ident!Unknown|Any `= value: AnyExpr!AnyValue -> nil")\
    /* control flow */\
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
#define TYPE(NAME, ...) extern Type fun_##NAME;
BASE_TYPES
#undef TYPE
#define RULE(NAME, ...) extern Type fun_##NAME;
BASE_RULES
#undef RULE

#define TYPE(NAME, ENUM_NAME, ...) ID_##ENUM_NAME,
#define RULE(NAME, ENUM_NAME, ...) ID_##ENUM_NAME,
enum BaseTypeIDs { BASE_TYPES BASE_RULES };
#undef RULE
#undef TYPE

extern Lang fungus_lang;

void fungus_define_base(Names *);

void fungus_lang_init(Names *);
void fungus_lang_quit(void);

#endif
