#include <assert.h>

#include "parse.h"
#include "lex/char_classify.h"

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
const char *ATOM_NAME[ATOM_COUNT] = { ATOM_TYPES };
#undef X

static RExpr *new_atom(Bump *pool, Type type, RAtomType atom_type,
                       hsize_t start, hsize_t len) {
    RExpr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (RExpr){
        .type = type,
        .is_atom = true,
        .atom_type = atom_type,
        .tok_start = start,
        .tok_len = len
    };

    return expr;
}

static RExpr *new_rule(Bump *pool, const RuleTree *rt, Rule rule, RExpr **exprs,
                       size_t len) {
    RExpr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (RExpr){
        .type = Rule_typeof(rt, rule),
        .rule = rule,
        .exprs = exprs,
        .len = len
    };

    return expr;
}

static RExpr *rule_copy_of_slice(Bump *pool, const RuleTree *rt, Rule rule,
                                 RExpr **slice, size_t len) {
    RExpr **exprs = Bump_alloc(pool, len * sizeof(*exprs));

    for (size_t j = 0; j < len; ++j)
        exprs[j] = slice[j];

    return new_rule(pool, rt, rule, exprs, len);
}

// turns tokens -> list of exprs, separating `{` and `}` as symbols but not
// creating the tree yet
static void gen_flat_list(Vec *list, Bump *pool, const RuleTree *rules,
                          const TokBuf *tb) {
    RAtomType atomtype_of_lit[TOK_COUNT] = {
        [TOK_BOOL] = ATOM_BOOL,
        [TOK_INT] = ATOM_INT,
        [TOK_FLOAT] = ATOM_FLOAT,
        [TOK_STRING] = ATOM_STRING,
    };

    for (size_t i = 0; i < tb->len; ++i) {
        TokType toktype = tb->types[i];
        hsize_t start = tb->starts[i], len = tb->lens[i];

        assert(toktype != TOK_INVALID);

        if (toktype == TOK_SYMBOLS) {
            const char *text = &tb->file->text.str[start];

            if (text[0] == '`') {
                // escaped literal lexeme
                if (len == 1)
                    File_error_at(tb->file, start, len, "bare lexeme escape.");

                RExpr *expr =
                    new_atom(pool, rules->ty_literal, ATOM_LEXEME, start, len);

                Vec_push(list, expr);
            } else {
                // split symbols on `{` and `}`
                hsize_t last_split = 0;

                for (hsize_t i = 0; i < len; ++i) {
                    if (text[i] == '{' || text[i] == '}') {
                        if (i > last_split) {
                            RExpr *expr = new_atom(pool, rules->ty_lexeme,
                                                   ATOM_LEXEME,
                                                   start + last_split,
                                                   i - last_split);

                            Vec_push(list, expr);
                        }

                        RExpr *expr = new_atom(pool, rules->ty_lexeme, ATOM_LEXEME,
                                               start + i, 1);

                        Vec_push(list, expr);

                        last_split = i + 1;
                    }
                }

                if (last_split < len) {
                    RExpr *expr = new_atom(pool, rules->ty_lexeme, ATOM_LEXEME,
                                           start + last_split,
                                           len - last_split);


                    Vec_push(list, expr);
                }
            }
        } else if (toktype == TOK_WORD) {
            RExpr *expr =
                new_atom(pool, rules->ty_lexeme, ATOM_LEXEME, start, len);

            Vec_push(list, expr);
        } else {
            // direct token -> expr translation
            RExpr *expr = new_atom(pool, rules->ty_literal,
                                   atomtype_of_lit[toktype], start, len);

            Vec_push(list, expr);
        }
    }
}

static void verify_scopes(const Vec *list, const RuleTree *rules,
                          const File *file) {
    int level = 0;

    for (size_t i = 0; i < list->len; ++i) {
        RExpr *expr = list->data[i];

        if (expr->type.id == rules->ty_lexeme.id && expr->tok_len == 1) {
            char ch = file->text.str[expr->tok_start];

            if (ch == '{') {
                ++level;
            } else if (ch == '}') {
                if (--level < 0)
                    RExpr_error(file, expr, "unmatched curly.");
            }
        }
    }

    if (level > 0)
        File_error_from(file, File_eof(file), "unfinished scope at EOF.");
}

// takes a verified list from gen_flat_list and makes a scope tree
static RExpr *unflatten_list(Bump *pool, const RuleTree *rules,
                             const File *file, const Vec *list) {
    const char *text = file->text.str;

    Vec levels[MAX_SCOPE_STACK];
    size_t level = 0;

    levels[level++] = Vec_new();

    for (size_t i = 0; i < list->len; ++i) {
        RExpr *expr = list->data[i];

        if (expr->type.id == rules->ty_lexeme.id
         && expr->atom_type == ATOM_LEXEME) {
            if (text[expr->tok_start] == '{') {
                // create new scope vec
                levels[level++] = Vec_new();

                continue;
            } else if (text[expr->tok_start] == '}') {
                // turn scope vec into a scope on pool
                Vec *vec = &levels[--level];

                expr = rule_copy_of_slice(pool, rules, rules->rule_scope,
                                          (RExpr **)vec->data, vec->len);

                Vec_del(vec);
            }
        }

        Vec_push(&levels[level - 1], expr);
    }

    RExpr *tree = rule_copy_of_slice(pool, rules, rules->rule_scope,
                                     (RExpr **)levels[0].data, levels[0].len);

    Vec_del(&levels[0]);

    return tree;
}

static RExpr *gen_scope_tree(Bump *pool, const RuleTree *rules,
                             const TokBuf *tb) {
    Vec root_list = Vec_new();

    gen_flat_list(&root_list, pool, rules, tb);
    verify_scopes(&root_list, rules, tb->file);

    RExpr *expr = unflatten_list(pool, rules, tb->file, &root_list);

    Vec_del(&root_list);

    return expr;
}

// stage 2: rule parsing =======================================================

// translates `scope` into the target language, returns success
static bool translate_scope(Bump *pool, const File *f, const Lang *lang,
                            RExpr *scope) {
    assert(scope->type.id == lang->rules.ty_scope.id);

    const RuleTree *rules = &lang->rules;

    RExpr **exprs = scope->exprs;
    size_t len = scope->len;
    Vec list = Vec_new();

    for (size_t i = 0; i < len; ++i) {
        RExpr *expr = exprs[i];

        if (expr->type.id == rules->ty_lexeme.id) {
            // symbols must be split up and fully matched, words can be idents
            // or stay lexemes
            View tok = { &f->text.str[expr->tok_start], expr->tok_len };

            if (ch_is_symbol(tok.str[0])) {
                hsize_t sym_start = expr->tok_start;
                size_t match_len;

                // match as many syms as possible
                while (tok.len > 0
                    && (match_len = HashSet_longest(&lang->syms, &tok))) {
                    RExpr *sym = new_atom(pool, rules->ty_lexeme, ATOM_LEXEME,
                                          sym_start, match_len);

                    Vec_push(&list, sym);

                    tok.str += match_len;
                    tok.len -= match_len;
                    sym_start += match_len;
                }

                if (tok.len > 0) {
                    File_error_from(f, sym_start, "unknown symbol");

                    return false;
                }
            } else {
                Word word = Word_new(tok.str, tok.len);

                if (!HashSet_has(&lang->words, &word)) {
                    expr->type = rules->ty_ident;
                    expr->atom_type = ATOM_IDENT;
                }

                Vec_push(&list, expr);
            }
        } else {
            Vec_push(&list, expr);
        }
    }

    scope->len = list.len;
    scope->exprs = Bump_alloc(pool, scope->len * sizeof(*scope->exprs));

    for (size_t i = 0; i < list.len; ++i)
        scope->exprs[i] = list.data[i];

    Vec_del(&list);

    return true;
}

// tries to match and collapse a rule on a slice, returns NULL if no match found
static RExpr *try_match(Bump *pool, const File *f, const RuleTree *rules,
                        RExpr **slice, size_t len, size_t *o_match_len) {
    // match a rule
    RuleNode *trav = rules->root;
    Rule best;
    size_t best_len = 0;

    for (size_t i = 0; i < len; ++i) {
        // match next node
        RExpr *expr = slice[i];
        RuleNode *next = NULL;

        if (expr->type.id == rules->ty_lexeme.id) {
            // match lexeme
            View token = { &f->text.str[expr->tok_start], expr->tok_len };

            for (size_t j = 0; j < trav->next_atoms.len; ++j) {
                const MatchAtom *match = trav->next_atoms.data[j];

                if (match->type == MATCH_LEXEME
                 && Word_eq_view(match->lxm, &token)) {
                    next = trav->next_nodes.data[j];
                    break;
                }
            }
        } else {
            // match a rule typeexpr
            for (size_t j = 0; j < trav->next_atoms.len; ++j) {
                const MatchAtom *match = trav->next_atoms.data[j];

                if (match->type == MATCH_EXPR
                 && Type_matches(&rules->types, expr->type, match->rule_expr)) {
                    next = trav->next_nodes.data[j];
                    break;
                }
            }
        }

        // no nodes matched
        if (!next)
            break;

        // nodes matched
        if (next->has_rule) {
            best = next->rule;
            best_len = i + 1;
        }

        trav = next;
    }

    // return match if found
    if (!best_len)
        return NULL;

    *o_match_len = best_len;

    return rule_copy_of_slice(pool, rules, best, slice, best_len);
}

static RExpr **lhs_of(RExpr *expr) {
    return &expr->exprs[0];
}

static RExpr **rhs_of(RExpr *expr) {
    return &expr->exprs[expr->len - 1];
}

typedef enum { LEFT, RIGHT } RotDir;

static bool expr_precedes(const Lang *lang, RotDir dir, RExpr *a, RExpr *b) {
    Prec expr_prec = Rule_get(&lang->rules, a->rule)->prec;
    Prec pivot_prec = Rule_get(&lang->rules, b->rule)->prec;

    Comparison cmp = Prec_cmp(&lang->precs, expr_prec, pivot_prec);

    bool prec_should_swap = cmp == GT;

    if (!prec_should_swap && cmp == EQ) {
        prec_should_swap = dir == RIGHT
            ? Prec_assoc(&lang->precs, expr_prec) == ASSOC_RIGHT
            : Prec_assoc(&lang->precs, expr_prec) == ASSOC_LEFT;
    }

    return prec_should_swap;
}

// returns if rotated
static bool try_rotate(const Lang *lang, RExpr *expr, RotDir dir,
                       RExpr **o_expr) {
    typedef RExpr **(*side_of_func)(RExpr *);
    side_of_func from_side = dir == RIGHT ? lhs_of : rhs_of;
    side_of_func to_side = dir == RIGHT ? rhs_of : lhs_of;

    // grab pivot, check if it should swap
    RExpr **pivot = from_side(expr);

    if ((*pivot)->is_atom || !expr_precedes(lang, dir, expr, *pivot))
        return false;

    // grab swap node, check if it could swap
    RExpr **swap = to_side(*pivot);

    while (!(*swap)->is_atom && expr_precedes(lang, dir, expr, *swap))
        swap = to_side(*swap);

    if ((*swap)->type.id == lang->rules.ty_lexeme.id)
        return false;

    // perform the swap
    *o_expr = *pivot;

    RExpr *mid = *swap;
    *swap = expr;
    *pivot = mid;

    return true;
}

/*
 * given a just-collapsed rule expr, checks if it needs to be rearranged to
 * respect precedence rules and rearranges
 */
static RExpr *correct_precedence(const Lang *lang, RExpr *expr) {
    RExpr *corrected;

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
static RExpr **collapse_rules(Bump *pool, const File *f, const Lang *lang,
                             RExpr **slice, size_t *io_len) {
    const RuleTree *rules = &lang->rules;
    size_t len = *io_len;

    // loop until a full passthrough of slice fails to match any rules
    bool matched;

    do {
        matched = false;

        for (size_t i = 0; i < len;) {
            // try to match rules on this index until none can be matched
            size_t match_len;
            RExpr *found =
                try_match(pool, f, rules, &slice[i], len - i, &match_len);

            if (!found) {
                ++i;
                continue;
            }

            found = correct_precedence(lang, found);

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

/*
 * given a raw scope, parse it using a lang
 */
RExpr *parse_scope(Bump *pool, const File *f, const Lang *lang, RExpr *expr) {
    if (!translate_scope(pool, f, lang, expr))
        return NULL;

    expr->exprs = collapse_rules(pool, f, lang, expr->exprs, &expr->len);

    return expr;
}

// general =====================================================================

static size_t ast_used_memory(RExpr *expr) {
    size_t used = sizeof(*expr);

    if (!expr->is_atom) {
        used += expr->len * sizeof(*expr->exprs);

        for (size_t i = 0; i < expr->len; ++i)
            used += ast_used_memory(expr->exprs[i]);
    }

    return used;
}

RExpr *parse(Bump *pool, const Lang *lang, const TokBuf *tb) {
    RExpr *ast = parse_scope(pool, tb->file, lang,
                             gen_scope_tree(pool, &lang->rules, tb));

    if (!ast)
        global_error = true;

#ifdef DEBUG
    if (ast) {
        printf("ast used/total allocated memory: %zu/%zu\n",
               ast_used_memory(ast), pool->total);
    }
#endif

    return ast;
}

static hsize_t RExpr_tok_start(const RExpr *expr) {
    while (!expr->is_atom)
        expr = expr->exprs[0];

    return expr->tok_start;
}

static hsize_t RExpr_tok_len(const RExpr *expr) {
    hsize_t start = RExpr_tok_start(expr);

    while (!expr->is_atom)
        expr = expr->exprs[expr->len - 1];

    return expr->tok_start + expr->tok_len - start;
}

void RExpr_error(const File *f, const RExpr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_at(f, RExpr_tok_start(expr), RExpr_tok_len(expr), fmt, args);
    va_end(args);
}

void RExpr_error_from(const File *f, const RExpr *expr, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_from(f, RExpr_tok_start(expr), fmt, args);
    va_end(args);
}

void RExpr_dump(const RExpr *expr, const Lang *lang, const File *file) {
    const char *text = file->text.str;
    const RExpr *scopes[MAX_SCOPE_STACK];
    size_t indices[MAX_SCOPE_STACK];
    size_t size = 0;

    while (true) {
        // print levels
        printf(TC_DIM);

        if (size > 0) {
            for (size_t i = 0; i < size - 1; ++i) {
                const char *c = "│";

                if (indices[i] == scopes[i]->len)
                    c = " ";

                printf("%s  ", c);
            }

            const char *c = indices[size - 1] == scopes[size - 1]->len
                ? "*" : "|";

            printf("%s── ", c);
        }

        printf(TC_RESET);

        // print name
        if (!expr->is_atom) {
            const Word *name = Rule_get(&lang->rules, expr->rule)->name;

            printf(TC_RED "%.*s" TC_RESET, (int)name->len, name->str);
        }

        // print expr
        if (!expr->is_atom) {
            scopes[size] = expr;
            indices[size] = 0;
            ++size;
        } else if (expr->type.id == lang->rules.ty_lexeme.id
                && expr->atom_type == ATOM_LEXEME) {
            // lexeme
            printf("%.*s", (int)expr->tok_len, &text[expr->tok_start]);
        } else if (expr->type.id == lang->rules.ty_literal.id
                && expr->atom_type == ATOM_LEXEME) {
            // lexeme literal
            printf("`" TC_GREEN "%.*s" TC_RESET,
                   (int)expr->tok_len - 1, &text[expr->tok_start + 1]);
        } else {
            switch (expr->atom_type) {
            case ATOM_IDENT:  printf(TC_BLUE);    break;
            default:          printf(TC_MAGENTA); break;
            }

            printf("%.*s" TC_RESET, (int)expr->tok_len, &text[expr->tok_start]);
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
