#include <assert.h>

#include "parse.h"

/*
 * parsing works in two stages:
 * 1. turn scopes (`{ ... }`) into a tree. this is the only parsing rule that is
 *    truly hardcoded into Fungus
 * 2. recursively parse scopes given a language context. this requires
 *    integration with semantic analysis, because `language`s must be parsed
 *    before they can be used for parsing
 */

// stage 1: scope tree =========================================================

// TODO get rid of this
#define MAX_SCOPE_STACK 1024

#define X(A) #A,
const char *EX_NAME[EX_COUNT] = { EXPR_TYPES };
#undef X

static Expr *new_expr(Bump *pool, ExprType type, hsize_t start, hsize_t len) {
    Expr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (Expr){
        .type = type,
        .tok_start = start,
        .tok_len = len
    };

    return expr;
}

static Expr *new_scope(Bump *pool, Expr **exprs, size_t len) {
    Expr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (Expr){
        .type = EX_SCOPE,
        .exprs = exprs,
        .len = len
    };

    return expr;
}

static Expr *scope_copy_of_slice(Bump *pool, Expr **slice, size_t len) {
    Expr **exprs = Bump_alloc(pool, len * sizeof(*exprs));

    for (size_t j = 0; j < len; ++j)
        exprs[j] = slice[j];

    return new_scope(pool, exprs, len);
}

static Expr *rule_copy_of_slice(Bump *pool, Rule rule, Expr **slice,
                                size_t len) {
    Expr *expr = Bump_alloc(pool, sizeof(*expr));
    Expr **exprs = Bump_alloc(pool, len * sizeof(*exprs));

    for (size_t j = 0; j < len; ++j)
        exprs[j] = slice[j];

    *expr = (Expr){
        .type = EX_RULE,
        .exprs = exprs,
        .len = len,
        .rule = rule
    };

    return expr;
}

// turns tokens -> list of exprs, separating `{` and `}` as symbols but not
// creating the tree yet
static void gen_flat_list(Vec *list, Bump *pool, const TokBuf *tb) {
    ExprType convert_toktype[TOK_COUNT] = {
        [TOK_INVALID] = EX_INVALID,
        [TOK_WORD] = EX_LEXEME,
        [TOK_BOOL] = EX_LIT_BOOL,
        [TOK_INT] = EX_LIT_INT,
        [TOK_FLOAT] = EX_LIT_FLOAT,
        [TOK_STRING] = EX_LIT_STRING,
    };

    for (size_t i = 0; i < tb->len; ++i) {
        TokType toktype = tb->types[i];

        if (toktype == TOK_SYMBOLS) {
            // split symbols on `{` and `}`
            hsize_t start = tb->starts[i], len = tb->lens[i];
            const char *text = &tb->file->text.str[start];
            hsize_t last_split = 0;

            for (hsize_t i = 0; i < len; ++i) {
                if (text[i] == '{' || text[i] == '}') {
                    if (i > last_split) {
                        Expr *expr = new_expr(pool, EX_LEXEME,
                                              start + last_split,
                                              i - last_split);

                        Vec_push(list, expr);
                    }

                    Vec_push(list, new_expr(pool, EX_LEXEME, start + i, 1));

                    last_split = i + 1;
                }
            }

            if (last_split < len) {
                Expr *expr = new_expr(pool, EX_LEXEME, start + last_split,
                                      len - last_split);

                Vec_push(list, expr);
            }
        } else {
            // direct token -> expr translation
            Expr *expr = new_expr(pool, convert_toktype[toktype], tb->starts[i],
                                  tb->lens[i]);

            Vec_push(list, expr);
        }
    }
}

static void verify_scopes(const Vec *list, const File *file) {
    int level = 0;

    for (size_t i = 0; i < list->len; ++i) {
        Expr *expr = list->data[i];

        if (expr->type == EX_LEXEME && expr->tok_len == 1) {
            char ch = file->text.str[expr->tok_start];

            if (ch == '{') {
                ++level;
            } else if (ch == '}') {
                if (--level < 0)
                    Expr_error(file, expr, "unmatched curly.");
            }
        }
    }

    if (level > 0)
        File_error_from(file, File_eof(file), "unfinished scope at EOF.");
}

// takes a verified list from gen_flat_list and makes a scope tree
static Expr *unflatten_list(Bump *pool, const File *file, const Vec *list) {
    const char *text = file->text.str;

    Vec levels[MAX_SCOPE_STACK];
    size_t level = 0;

    levels[level++] = Vec_new();

    for (size_t i = 0; i < list->len; ++i) {
        Expr *expr = list->data[i];

        if (expr->type == EX_LEXEME) {
            if (text[expr->tok_start] == '{') {
                // create new scope vec
                levels[level++] = Vec_new();

                continue;
            } else if (text[expr->tok_start] == '}') {
                // turn scope vec into a scope on pool
                Vec *vec = &levels[--level];

                expr = scope_copy_of_slice(pool, (Expr **)vec->data, vec->len);

                Vec_del(vec);
            }
        }

        Vec_push(&levels[level - 1], expr);
    }

    Expr *tree =
        scope_copy_of_slice(pool, (Expr **)levels[0].data, levels[0].len);

    Vec_del(&levels[0]);

    return tree;
}

static Expr *gen_scope_tree(Bump *pool, const TokBuf *tb) {
    Vec root_list = Vec_new();

    gen_flat_list(&root_list, pool, tb);
    verify_scopes(&root_list, tb->file);

    Expr *expr = unflatten_list(pool, tb->file, &root_list);

    Vec_del(&root_list);

    return expr;
}

// stage 2: rule parsing =======================================================

// tries to match and collapse a rule on a slice, returns NULL if no match found
static Expr *try_match(Bump *pool, const File *f, const RuleTree *rules,
                       Expr **slice, size_t len, size_t *o_match_len) {
    // match a rule
    RuleNode *trav = rules->root;
    Rule best;
    size_t best_len = 0;

    for (size_t i = 0; i < len; ++i) {
        // match next node
        Expr *expr = slice[i];
        RuleNode *next = NULL;

        if (expr->type == EX_LEXEME) {
            // match lexeme
            View token = { &f->text.str[expr->tok_start], expr->tok_len };

            for (size_t j = 0; j < trav->num_next_lxms; ++j) {
                if (Word_eq_view(trav->next_lxm_type[j], &token)) {
                    next = trav->next_lxm[j];
                    break;
                }
            }
        } else {
            // match anything
            next = trav->next_expr;
        }

        // no nodes matched
        if (!next)
            break;

        // nodes matched
        if (Rule_is_valid(next->rule)) {
            best = next->rule;
            best_len = i + 1;
        }

        trav = next;
    }

    // return match if found
    if (!best_len)
        return NULL;

    *o_match_len = best_len;

    return rule_copy_of_slice(pool, best, slice, best_len);
}

static Expr **lhs_of(Expr *expr) {
    assert(expr->type == EX_RULE);

    return &expr->exprs[0];
}

static Expr **rhs_of(Expr *expr) {
    assert(expr->type == EX_RULE);

    return &expr->exprs[expr->len - 1];
}

typedef Expr **(*side_of_func)(Expr *);

typedef enum { LEFT, RIGHT } RotDir;

// returns if rotated
static bool try_rotate(const Lang *lang, Expr *expr, RotDir dir,
                       Expr **o_expr) {
    assert(expr->type == EX_RULE);

    side_of_func from_side = dir == RIGHT ? lhs_of : rhs_of;
    side_of_func to_side = dir == RIGHT ? rhs_of : lhs_of;

    printf("trying rotate %s\n", dir ? "r" : "l");

    // grab pivot, check if it should swap
    Expr **pivot = from_side(expr);

    if ((*pivot)->type != EX_RULE)
        return false;

    puts("pivot could swap");

    // grab swap node, check if it could swap
    Expr **swap = to_side(*pivot);

    if ((*swap)->type == EX_LEXEME)
        return false;

    puts("swap could swap");

    // check if precedence necessitates a swap
    Prec expr_prec = Rule_get(&lang->rules, expr->rule)->prec;
    Prec pivot_prec = Rule_get(&lang->rules, (*pivot)->rule)->prec;

    Comparison cmp = Prec_cmp(&lang->precs, expr_prec, pivot_prec);

    bool prec_should_swap = cmp == GT;

    if (!prec_should_swap && cmp == EQ) {
        prec_should_swap = dir == RIGHT
            ? Prec_assoc(&lang->precs, expr_prec) == ASSOC_RIGHT
            : Prec_assoc(&lang->precs, expr_prec) == ASSOC_LEFT;
    }

    if (!prec_should_swap)
        return false;

    puts("prec should swap");

    // perform the swap
    *o_expr = *pivot;

    Expr *mid = *swap;
    *swap = expr;
    *pivot = mid;

    return true;
}

/*
 * given a just-collapsed rule expr, checks if it needs to be rearranged to
 * respect precedence rules
 */
static Expr *correct_precedence(const Lang *lang, Expr *expr) {
    assert(expr->type == EX_RULE);

    Expr *corrected;

    if (try_rotate(lang, expr, RIGHT, &corrected)
     || try_rotate(lang, expr, LEFT, &corrected)) {
        return corrected;
    }

    return expr;
}

/*
 * heavily modifies `slice`. pass length in by pointer, after calling func the
 * returned slice will contain the fully collapsed scope and length will be
 * modified to reflect this
 */
static Expr **collapse_rules(Bump *pool, const File *f, const Lang *lang,
                             Expr **slice, size_t *io_len) {
    const RuleTree *rules = &lang->rules;
    size_t len = *io_len;

    // loop until a full passthrough of slice fails to match any rules
    bool matched;

    do {
        matched = false;

        size_t i = 0;

        while (i < len) {
            // try to match rules on this index until none can be matched
            size_t match_len;
            Expr *found =
                try_match(pool, f, rules, &slice[i], len - i, &match_len);

            if (!found) {
                ++i;
                continue;
            }


            puts("checking prec on:");
            Expr_dump(found, f);
            found = correct_precedence(lang, found);
            puts("corrected to:");
            Expr_dump(found, f);

            // stitch slice
            size_t move_len = match_len - 1;

            for (size_t j = 1; j <= i; ++j)
                slice[i + move_len - j] = slice[i - j];

            slice[i + move_len] = found;

            // fix slice ptr + len
            slice += move_len;
            len -= move_len;

            matched = true;
        }
    } while (matched);

    *io_len = len;

    return slice;
}

Expr *parse_scope(Bump *pool, const File *f, const Lang *lang, Expr *expr) {
    assert(expr->type == EX_SCOPE);

    expr->exprs = collapse_rules(pool, f, lang, expr->exprs, &expr->len);

    return expr;
}

// general =====================================================================

Expr *parse(Bump *pool, const Lang *lang, const TokBuf *tb) {
    return parse_scope(pool, tb->file, lang, gen_scope_tree(pool, tb));
}

static hsize_t Expr_tok_start(const Expr *expr) {
    while (expr->type == EX_SCOPE || expr->type == EX_RULE)
        expr = expr->exprs[0];

    return expr->tok_start;
}

static hsize_t Expr_tok_len(const Expr *expr) {
    hsize_t start = Expr_tok_start(expr);

    while (expr->type == EX_SCOPE || expr->type == EX_RULE)
        expr = expr->exprs[expr->len - 1];

    return expr->tok_start + expr->tok_len - start;
}

void Expr_error(const File *f, const Expr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_at(f, Expr_tok_start(expr), Expr_tok_len(expr), fmt, args);
    va_end(args);
}

void Expr_error_from(const File *f, const Expr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_from(f, Expr_tok_start(expr), fmt, args);
    va_end(args);
}

void Expr_dump(const Expr *expr, const File *file) {
    const char *text = file->text.str;
    const Expr *scopes[MAX_SCOPE_STACK];
    size_t indices[MAX_SCOPE_STACK];
    size_t size = 0;

    while (true) {
        // print levels
        if (size > 0) {
            for (size_t i = 0; i < size - 1; ++i) {
                const char *c = "│";

                if (indices[i] == scopes[i]->len)
                    c = " ";

                printf("%s   ", c);
            }

            const char *c = indices[size - 1] == scopes[size - 1]->len
                ? "*" : "|";

            printf("%s── ", c);
        }

        printf("%-10s", EX_NAME[expr->type]);

        // print expr
        if (expr->type == EX_SCOPE || expr->type == EX_RULE) {
            scopes[size] = expr;
            indices[size] = 0;
            ++size;
        } else {
            printf(" `%.*s`", (int)expr->tok_len,
                   &text[expr->tok_start]);
        }

        puts("");

        // get next expr
        while (size > 0 && indices[size - 1] >= scopes[size - 1]->len)
            --size;

        if (!size)
            break;

        expr = scopes[size - 1]->exprs[indices[size - 1]++];
    }
}
