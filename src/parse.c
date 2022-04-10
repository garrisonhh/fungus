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

#define X(A) #A,
const char *ATOM_NAME[ATOM_COUNT] = { ATOM_TYPES };
#undef X

static AstExpr *new_atom(Bump *pool, Type type, AstAtomType atom_type,
                         hsize_t start, hsize_t len) {
    AstExpr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (AstExpr){
        .type = type,
        .is_atom = true,
        .atom_type = atom_type,
        .tok_start = start,
        .tok_len = len
    };

    return expr;
}

static AstExpr *new_rule(Bump *pool, const RuleTree *rt, Rule rule,
                         AstExpr **exprs, size_t len) {
    AstExpr *expr = Bump_alloc(pool, sizeof(*expr));

    *expr = (AstExpr){
        .type = Rule_typeof(rt, rule),
        .rule = rule,
        .exprs = exprs,
        .len = len
    };

    return expr;
}

static AstExpr *rule_copy_of_slice(Bump *pool, const RuleTree *rt, Rule rule,
                                 AstExpr **slice, size_t len) {
    AstExpr **exprs = Bump_alloc(pool, len * sizeof(*exprs));

    for (size_t j = 0; j < len; ++j)
        exprs[j] = slice[j];

    return new_rule(pool, rt, rule, exprs, len);
}

// turns tokens -> list of exprs, separating `{` and `}` as symbols but not
// creating the tree yet
static void gen_flat_list(Vec *list, Bump *pool, const RuleTree *rules,
                          const TokBuf *tb) {
    AstAtomType atomtype_of_lit[TOK_COUNT] = {
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

                AstExpr *expr =
                    new_atom(pool, rules->ty_literal, ATOM_LEXEME, start, len);

                Vec_push(list, expr);
            } else {
                // split symbols on `{` and `}`
                hsize_t last_split = 0;

                for (hsize_t i = 0; i < len; ++i) {
                    if (text[i] == '{' || text[i] == '}') {
                        if (i > last_split) {
                            AstExpr *expr = new_atom(pool, rules->ty_lexeme,
                                                     ATOM_LEXEME,
                                                     start + last_split,
                                                     i - last_split);

                            Vec_push(list, expr);
                        }

                        AstExpr *expr = new_atom(pool, rules->ty_lexeme,
                                                 ATOM_LEXEME,
                                                 start + i, 1);

                        Vec_push(list, expr);

                        last_split = i + 1;
                    }
                }

                if (last_split < len) {
                    AstExpr *expr = new_atom(pool, rules->ty_lexeme,
                                             ATOM_LEXEME,
                                             start + last_split,
                                             len - last_split);


                    Vec_push(list, expr);
                }
            }
        } else if (toktype == TOK_WORD) {
            AstExpr *expr =
                new_atom(pool, rules->ty_lexeme, ATOM_LEXEME, start, len);

            Vec_push(list, expr);
        } else {
            // direct token -> expr translation
            AstExpr *expr = new_atom(pool, rules->ty_literal,
                                   atomtype_of_lit[toktype], start, len);

            Vec_push(list, expr);
        }
    }
}

static void verify_scopes(const Vec *list, const RuleTree *rules,
                          const File *file) {
    int level = 0;

    for (size_t i = 0; i < list->len; ++i) {
        AstExpr *expr = list->data[i];

        if (expr->type.id == rules->ty_lexeme.id && expr->tok_len == 1) {
            char ch = file->text.str[expr->tok_start];

            if (ch == '{') {
                ++level;
            } else if (ch == '}') {
                if (--level < 0)
                    AstExpr_error(file, expr, "unmatched curly.");
            }
        }
    }

    if (level > 0)
        File_error_from(file, File_eof(file), "unfinished scope at EOF.");
}

// takes a verified list from gen_flat_list and makes a scope tree
static AstExpr *unflatten_list(Bump *pool, const RuleTree *rules,
                             const File *file, const Vec *list) {
    const char *text = file->text.str;

    Vec levels[MAX_AST_DEPTH];
    size_t level = 0;

    levels[level++] = Vec_new();

    for (size_t i = 0; i < list->len; ++i) {
        AstExpr *expr = list->data[i];

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
                                          (AstExpr **)vec->data, vec->len);

                Vec_del(vec);
            }
        }

        Vec_push(&levels[level - 1], expr);
    }

    AstExpr *tree = rule_copy_of_slice(pool, rules, rules->rule_scope,
                                       (AstExpr **)levels[0].data,
                                       levels[0].len);

    Vec_del(&levels[0]);

    return tree;
}

static AstExpr *gen_scope_tree(Bump *pool, const RuleTree *rules,
                             const TokBuf *tb) {
    Vec root_list = Vec_new();

    gen_flat_list(&root_list, pool, rules, tb);
    verify_scopes(&root_list, rules, tb->file);

    AstExpr *expr = unflatten_list(pool, rules, tb->file, &root_list);

    Vec_del(&root_list);

    return expr;
}

// stage 2: rule parsing =======================================================

// translates `scope` into the target language, returns success
static bool translate_scope(Bump *pool, const File *f, const Lang *lang,
                            AstExpr *scope) {
    assert(scope->type.id == lang->rules.ty_scope.id);

    const RuleTree *rules = &lang->rules;

    AstExpr **exprs = scope->exprs;
    size_t len = scope->len;
    Vec list = Vec_new();

    for (size_t i = 0; i < len; ++i) {
        AstExpr *expr = exprs[i];

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
                    AstExpr *sym = new_atom(pool, rules->ty_lexeme, ATOM_LEXEME,
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

static bool expr_matches(const File *f, const RuleTree *rules,
                         const AstExpr *expr, const MatchAtom *pred) {
    bool matches = false;

    if (expr->type.id == rules->ty_lexeme.id) {
        // lexeme
        View token = { &f->text.str[expr->tok_start], expr->tok_len };

        matches = pred->type == MATCH_LEXEME && Word_eq_view(pred->lxm, &token);
    } else {
        // expr
        matches = pred->type == MATCH_EXPR
               && Type_matches(&rules->types, expr->type, pred->rule_expr);
    }

    return matches;
}

// find index of first match within a Vec<RuleNode *>
static bool expr_match_of(const File *f, const RuleTree *rules,
                          const AstExpr *expr, const Vec *nodes,
                          size_t *o_idx) {
    if (expr->type.id == rules->ty_lexeme.id) {
        // match lexeme
        View token = { &f->text.str[expr->tok_start], expr->tok_len };

        for (size_t i = 0; i < nodes->len; ++i) {
            const MatchAtom *pred = ((RuleNode *)nodes->data[i])->pred;

            if (pred->type == MATCH_LEXEME
             && Word_eq_view(pred->lxm, &token)) {
                *o_idx = i;

                return true;
            }
        }
    } else {
        // match a rule typeexpr
        for (size_t i = 0; i < nodes->len; ++i) {
            const MatchAtom *pred = ((RuleNode *)nodes->data[i])->pred;

            if (pred->type == MATCH_EXPR
             && Type_matches(&rules->types, expr->type, pred->rule_expr)) {
                *o_idx = i;

                return true;
            }
        }
    }

    return false;
}

static AstExpr *try_match(Bump *, const File *, const RuleTree *, AstExpr **,
                          size_t *, size_t *);

static AstExpr *try_backtrack(Bump *pool, const File *f, const RuleTree *rules,
                              AstExpr **slice, size_t *io_start,
                              size_t *io_len) {
    size_t start = *io_start, len = *io_len;
    AstExpr *first = slice[start];

    // find backtrack
    size_t idx;

    if (!expr_match_of(f, rules, first, &rules->roots, &idx))
        return NULL;

    RuleBacktrack *bt = rules->backtracks.data[idx];
    AstExpr *prev = slice[start - 1];

    if (!expr_match_of(f, rules, prev, &bt->backs, &idx))
        return NULL;

    // backtrack possible, try to match on it
    size_t prev_start = start - 1, prev_len = len + 1;
    AstExpr *prev_match = try_match(pool, f, rules, slice,
                                  &prev_start, &prev_len);

    if (prev_match) {
        // found a previous match!
        *io_start = prev_start;
        *io_len = prev_len;

        return prev_match;
    }

    return NULL;
}

// call with depth = 0 && *io_max_depth = 0
// if io_max_depth is still 0 once called, the Rule will not be valid
static Rule try_match_r(const File *f, const RuleTree *rules, const Vec *nexts,
                        AstExpr **slice, size_t len, size_t depth,
                        size_t *io_max_depth) {
    Rule best = {0};

    if (len == 0)
        return best;

    const AstExpr *expr = slice[0];

    for (size_t i = 0; i < nexts->len; ++i) {
        RuleNode *node = nexts->data[i];

        if (expr_matches(f, rules, expr, node->pred)) {
            // check if this is the best match yet
            if (node->has_rule && depth > *io_max_depth) {
                *io_max_depth = depth;
                best = node->rule;
            }

            // match on node's children
            size_t next_depth = *io_max_depth;
            Rule next_best = try_match_r(f, rules, &node->nexts, slice + 1,
                                         len - 1, depth + 1, &next_depth);

            if (next_depth > *io_max_depth) {
                *io_max_depth = next_depth;
                best = next_best;
            }
        }
    }

    return best;
}

// tries to match and collapse a rule on a slice, returns NULL if no match found
static AstExpr *try_match(Bump *pool, const File *f, const RuleTree *rules,
                          AstExpr **slice, size_t *io_start, size_t *io_len) {
    size_t start = *io_start, len = *io_len;

    // check backtracking
    if (start > 0) {
        AstExpr *match = try_backtrack(pool, f, rules, slice, &start, &len);

        if (match) {
            *io_start = start;
            *io_len = len;

            return match;
        }
    }

    // match a rule
    size_t best_depth = 0;
    Rule best_rule = try_match_r(f, rules, &rules->roots, &slice[start],
                                 len - start, 0, &best_depth);

    // return match if found
    if (!best_depth)
        return NULL;

    *io_start = start;
    *io_len = best_depth + 1;

    return rule_copy_of_slice(pool, rules, best_rule, &slice[start],
                              best_depth + 1);
}

static AstExpr **lhs_of(AstExpr *expr) {
    return &expr->exprs[0];
}

static AstExpr **rhs_of(AstExpr *expr) {
    return &expr->exprs[expr->len - 1];
}

typedef enum { LEFT, RIGHT } RotDir;

static bool expr_precedes(const Lang *lang, RotDir dir, AstExpr *a, AstExpr *b) {
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
static bool try_rotate(const Lang *lang, AstExpr *expr, RotDir dir,
                       AstExpr **o_expr) {
    typedef AstExpr **(*side_of_func)(AstExpr *);
    side_of_func from_side = dir == RIGHT ? lhs_of : rhs_of;
    side_of_func to_side = dir == RIGHT ? rhs_of : lhs_of;

    // grab pivot, check if it should swap
    AstExpr **pivot = from_side(expr);

    if ((*pivot)->is_atom || !expr_precedes(lang, dir, expr, *pivot))
        return false;

    // grab swap node, check if it could swap
    AstExpr **swap = to_side(*pivot);

    while (!(*swap)->is_atom && expr_precedes(lang, dir, expr, *swap))
        swap = to_side(*swap);

    if ((*swap)->type.id == lang->rules.ty_lexeme.id)
        return false;

    // perform the swap
    *o_expr = *pivot;

    AstExpr *mid = *swap;
    *swap = expr;
    *pivot = mid;

    return true;
}

/*
 * given a just-collapsed rule expr, checks if it needs to be rearranged to
 * respect precedence rules and rearranges
 */
static AstExpr *correct_precedence(const Lang *lang, AstExpr *expr) {
    AstExpr *corrected;

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
static AstExpr **collapse_rules(Bump *pool, const File *f, const Lang *lang,
                              AstExpr **slice, size_t *io_len) {
    const RuleTree *rules = &lang->rules;
    size_t len = *io_len;

    // loop until a full passthrough of slice fails to match any rules
    bool matched;

    do {
        matched = false;

        for (size_t i = 0; i < len;) {
            // try to match rules on this index until none can be matched
            size_t match_start = i, match_len = len;
            AstExpr *found =
                try_match(pool, f, rules, slice, &match_start, &match_len);

            if (!found) {
                ++i;
                continue;
            }

            matched = true;
            found = correct_precedence(lang, found);

            // stitch slice
            size_t move_len = match_len - 1;

            for (size_t j = 1; j <= match_start; ++j)
                slice[match_start + move_len - j] = slice[match_start - j];

            slice[match_start + move_len] = found;

            slice += move_len;
            len -= move_len;

#ifdef DEBUG
            puts("AFTER MATCH");
            for (size_t j = 0; j < len; ++j)
                AstExpr_dump(slice[j], lang, f);
#endif
        }
    } while (matched);

    *io_len = len;

    return slice;
}

/*
 * given a raw scope, parse it using a lang
 */
AstExpr *parse_scope(Bump *pool, const File *f, const Lang *lang, AstExpr *expr) {
    if (!translate_scope(pool, f, lang, expr))
        return NULL;

    expr->exprs = collapse_rules(pool, f, lang, expr->exprs, &expr->len);

    return expr;
}

// general =====================================================================

static size_t ast_used_memory(AstExpr *expr) {
    size_t used = sizeof(*expr);

    if (!expr->is_atom) {
        used += expr->len * sizeof(*expr->exprs);

        for (size_t i = 0; i < expr->len; ++i)
            used += ast_used_memory(expr->exprs[i]);
    }

    return used;
}

AstExpr *parse(Bump *pool, const Lang *lang, const TokBuf *tb) {
#ifdef DEBUG
    double start = time_now();
#endif

    AstExpr *ast = parse_scope(pool, tb->file, lang,
                             gen_scope_tree(pool, &lang->rules, tb));

    if (!ast)
        global_error = true;

#ifdef DEBUG
    if (ast) {
        printf("ast used/total allocated memory: %zu/%zu\n",
               ast_used_memory(ast), pool->total);
    }

    double duration = time_now() - start;

    printf("parsing took %.6fs.\n", duration);
#endif

    return ast;
}
