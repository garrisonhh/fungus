#include <inttypes.h>

#include "sir.h"
#include "fungus.h"
#include "utils.h"

// see SirExpr_dump
char SE_NAMES[SE_COUNT][MAX_NAME_LEN];

static SirType SirType_new_atom(SirTypeType type) {
    return (SirType){ .type = type };
}

static SirExpr *SirExpr_new(Bump *pool, SirExprType type) {
    SirExpr *sir = Bump_alloc(pool, sizeof(*sir));

    sir->type = type;

    return sir;
}

static SirType convert_type(Type type) {
    switch (type.id) {
#define ATOM_CASE(A, B) case A: return SirType_new_atom(B)
    ATOM_CASE(ID_NIL,   ST_NIL);
    ATOM_CASE(ID_BOOL,  ST_BOOL);
    ATOM_CASE(ID_INT,   ST_I64);
    ATOM_CASE(ID_FLOAT, ST_F64);
#undef ATOM_CASE
    default: {
        const Word *name = Type_name(type);
        fungus_panic("cannot convert type `%.*s` to sir",
                     (int)name->len, name->str);
    }
    }
}

static SirExpr *SirExpr_new_op(Bump *pool, SirExprType type, SirType evaltype,
                               const SirExpr *a, const SirExpr *b,
                               const SirExpr *c) {
    SirExpr *sir = SirExpr_new(pool, type);

    sir->evaltype = evaltype;
    sir->a = a;
    sir->b = b;
    sir->c = c;

    return sir;
}

// mutually recursive with all the convert funcs
static const SirExpr *convert_ast(Bump *, const File *, const AstExpr *);

static SirExpr *convert_scope(Bump *pool, const File *file,
                              const AstExpr *expr) {
    assert(expr->type.id == ID_SCOPE);

    // convert children
    const SirExpr **children = Bump_alloc(pool, expr->len * sizeof(*children));

    for (size_t i = 0; i < expr->len; ++i)
        children[i] = convert_ast(pool, file, expr->exprs[i]);

    // return scope as SirExpr
    SirExpr *sir = SirExpr_new(pool, SE_SCOPE);

    sir->evaltype = expr->len
        ? children[expr->len - 1]->evaltype
        : SirType_new_atom(ST_NIL);
    sir->exprs = children;
    sir->len = expr->len;

    return sir;
}

static SirExpr *convert_literal(Bump *pool, const File *file,
                                const AstExpr *expr) {
    assert(expr->type.id == ID_LITERAL);

    const char *str = file->text.str;

    SirExpr *sir = SirExpr_new(pool, SE_LITERAL);

    switch (expr->evaltype.id) {
    case ID_BOOL:
        sir->evaltype = SirType_new_atom(ST_BOOL);
        sir->bool_ = str[expr->tok_start] == 't';
        break;
    case ID_INT: {
        // TODO don't use strtoll, please lmao
        char buf[256];

        for (size_t i = 0; i < expr->tok_len; ++i)
            buf[i] = str[expr->tok_start + i];

        buf[expr->tok_len] = '\0';

        char *end;
        long long res = strtoll(buf, &end, 10);

        assert(end == buf + expr->tok_len);

        sir->evaltype = SirType_new_atom(ST_I64);
        sir->i64 = res;
        break;
    }
    default:
        AstExpr_error(file, expr, "invalid literal for sir");
        exit(-1);
    }

    return sir;
}

static SirExpr *convert_binary_op(Bump *pool, const File *file,
                                  const AstExpr *expr) {
    SirExprType se_type;

    switch (expr->type.id) {
#define BINOP_CASE(A, B) case A: se_type = B; break
    BINOP_CASE(ID_ADD, SE_ADD);
    BINOP_CASE(ID_SUBTRACT, SE_SUB);
    BINOP_CASE(ID_MULTIPLY, SE_MUL);
    BINOP_CASE(ID_DIVIDE, SE_DIV);
    BINOP_CASE(ID_MODULO, SE_MOD);
#undef BINOP_CASE
    default: UNREACHABLE;
    }

    SirExpr *sir = SirExpr_new(pool, se_type);

    sir->evaltype = convert_type(expr->evaltype);
    sir->a = convert_ast(pool, file, expr->exprs[0]);
    sir->b = convert_ast(pool, file, expr->exprs[2]);

    return sir;
}

static const SirExpr *convert_ast(Bump *pool, const File *file,
                                  const AstExpr *expr) {
    switch (expr->type.id) {
    case ID_SCOPE:
        return convert_scope(pool, file, expr);
    case ID_LITERAL:
        return convert_literal(pool, file, expr);
    case ID_ADD:
    case ID_SUBTRACT:
    case ID_MULTIPLY:
    case ID_DIVIDE:
    case ID_MODULO:
        return convert_binary_op(pool, file, expr);
    default:
        break;
    }

    const Word *type_name = Type_name(expr->type);
    const Word *eval_name = Type_name(expr->evaltype);

    AstExpr_error(file, expr,
                  "unable to convert this expr to sir (types `%.*s!%.*s`)",
                  (int)type_name->len, type_name->str,
                  (int)eval_name->len, eval_name->str);
    exit(-1);
}

// TODO is this func necessary
const SirExpr *generate_sir(Bump *pool, const File *file, const AstExpr *ast) {
    return convert_ast(pool, file, ast);
}

#define INDENT 2

static void SirExpr_dump_r(const SirExpr *sir, int level) {
    printf("%*s" TC_RED "%s" TC_RESET "%s", level * INDENT, "",
           SE_NAMES[sir->type], sir->type == SE_LITERAL ? " " : "\n");

    switch (sir->type) {
    case SE_SCOPE:
        for (size_t i = 0; i < sir->len; ++i)
            SirExpr_dump_r(sir->exprs[i], level + 1);

        break;
    case SE_LITERAL:
        printf(TC_MAGENTA);

        switch (sir->evaltype.type) {
        case ST_BOOL:
            printf("%s", sir->bool_ ? "true" : "false");
            break;
        case ST_I64:
            printf("%" PRId64, sir->i64);
            break;
        case ST_F64:
            printf("%f", sir->f64);
            break;
        default:
            UNIMPLEMENTED;
        }

        puts(TC_RESET);
        break;
    case SE_ADD:
    case SE_SUB:
    case SE_MUL:
    case SE_DIV:
    case SE_MOD:
        SirExpr_dump_r(sir->a, level + 1);
        SirExpr_dump_r(sir->b, level + 1);
        break;
    default:
        UNIMPLEMENTED;
    }
}

void SirExpr_dump(const SirExpr *sir) {
    // TODO is this too hacky?
    static bool init = false;

    if (!init) {
#define X(NAME) #NAME,
        char *pre_se_names[] = { SIR_EXPR_TYPES };
#undef X

        names_to_lower(SE_NAMES, pre_se_names, SE_COUNT);
        init = true;
    }

    SirExpr_dump_r(sir, 0);
}

