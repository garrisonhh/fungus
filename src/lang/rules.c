#include <assert.h>

#include "rules.h"
#include "ast_expr.h"
#include "../fungus.h"

static void *RT_alloc(RuleTree *rt, size_t n_bytes) {
    return Bump_alloc(&rt->pool, n_bytes);
}

static RuleNode *RT_new_node(RuleTree *rt, MatchAtom *pred) {
    RuleNode *node = RT_alloc(rt, sizeof(*node));

    *node = (RuleNode){
        .pred = pred,
        .nexts = Vec_new()
    };

    return node;
}

// places RuleEntry in entries and idmap
static Rule register_entry(RuleTree *rt, RuleEntry *entry) {
    Rule handle = { rt->entries.len };

    Vec_push(&rt->entries, entry);
    IdMap_put(&rt->by_name, entry->name, handle.id);

    return handle;
}

RuleTree RuleTree_new(void) {
    RuleTree rt = {
        .pool = Bump_new(),
        .entries = Vec_new(),
        .by_name = IdMap_new(),
    };

    rt.roots = Vec_new();

    // Scope rule
    RuleEntry *scope_entry = RT_alloc(&rt, sizeof(*scope_entry));
    Word name = WORD("Scope");

    *scope_entry = (RuleEntry){
        .name = Word_copy_of(&name, &rt.pool),
        .type = fun_scope
    };

    rt.rule_scope = register_entry(&rt, scope_entry);

    return rt;
}

static void RuleNode_del(RuleNode *node) {
    for (size_t i = 0; i < node->nexts.len; ++i) {
        RuleNode *child = node->nexts.data[i];

        if (child != node)
            RuleNode_del(child);
    }

    Vec_del(&node->nexts);
}

void RuleTree_del(RuleTree *rt) {
    for (size_t i = 0; i < rt->roots.len; ++i)
        RuleNode_del(rt->roots.data[i]);

    IdMap_del(&rt->by_name);
    Vec_del(&rt->entries);
    Bump_del(&rt->pool);
}

static void place_rule_r(RuleTree *rt, RuleNode *node, Vec *nexts,
                         const Pattern *pat, size_t index, Rule rule) {
    assert(index <= pat->len);

    // place rule at end of pattern
    if (index == pat->len) {
        assert(node != NULL);

        node->rule = rule;
        node->has_rule = true;

        return;
    }

    // check for equivalent predicate within nexts
    MatchAtom *pred = &pat->matches[index];
    RuleNode *place = NULL;

    // place on optional, if set
    if (pred->type == MATCH_EXPR && pred->optional)
        place_rule_r(rt, node, nexts, pat, index + 1, rule);

    for (size_t i = 0; i < nexts->len; ++i) {
        RuleNode *next = nexts->data[i];
        bool matching = false;

        if (pred->type == next->pred->type) {
            switch (pred->type) {
            case MATCH_LEXEME:
                matching = Word_eq(pred->lxm, next->pred->lxm);
                break;
            case MATCH_EXPR:
                matching =
                    TypeExpr_equals(pred->rule_expr, next->pred->rule_expr);
                break;
            }
        }

        if (matching) {
            place = next;
            break;
        }
    }

    // no match found, create a new node
    if (!place) {
        place = RT_new_node(rt, pred);
        Vec_push(nexts, place); // TODO copy predicate here to ruletree pool

        // apply repeating flag, if set
        if (pred->type == MATCH_EXPR && pred->repeating)
            Vec_push(&place->nexts, place);
    }

    // recur
    place_rule_r(rt, place, &place->nexts, pat, index + 1, rule);
}

static void place_rule(RuleTree *rt, const Pattern *pat, Rule rule) {
    place_rule_r(rt, NULL, &rt->roots, pat, 0, rule);
}

Type Rule_define_type(Names *names, Word name) {
    return Type_define(names, name, &fun_rule, 1);
}

Rule Rule_immediate_define(RuleTree *rt, Type type, Prec prec, Pattern pat) {
    RuleEntry *entry = RT_alloc(rt, sizeof(*entry));

    *entry = (RuleEntry){
        .name = Word_copy_of(Type_name(type), &rt->pool),
        .pat = pat,
        .prec = prec,
        .type = type
    };

    Rule handle = register_entry(rt, entry);

    place_rule(rt, &pat, handle);

    return handle;
}

Rule Rule_define(RuleTree *rt, const File *file, Type type, Prec prec,
                 AstExpr *pat_ast) {
    assert(file && pat_ast);

    RuleEntry *entry = RT_alloc(rt, sizeof(*entry));

    *entry = (RuleEntry){
        .name = Word_copy_of(Type_name(type), &rt->pool),
        .pre_file = file,
        .pre_pat = pat_ast,
        .prec = prec,
        .type = type
    };

    return register_entry(rt, entry);
}

void RuleTree_crystallize(RuleTree *rt, Names *names) {
#ifdef DEBUG
    assert(!rt->crystallized);
#endif

    // compile and place all rules
    for (size_t i = 0; i < rt->entries.len; ++i) {
        if (i == rt->rule_scope.id)
            continue;

        RuleEntry *entry = rt->entries.data[i];

        DEBUG_SCOPE(0,
            puts(TC_CYAN "compiling pattern:" TC_RESET);
            AstExpr_dump(entry->pre_pat, &pattern_lang, entry->pre_file);
        );

        entry->pat =
            compile_pattern(&rt->pool, names, entry->pre_file, entry->pre_pat);

        DEBUG_SCOPE(0,
            puts(TC_CYAN "compiled into:" TC_RESET);
            Pattern_print(&entry->pat);
            puts("\n");
        );

        place_rule(rt, &entry->pat, Rule_by_name(rt, entry->name));
    }

#ifdef DEBUG
    rt->crystallized = true;
#endif
}

Type Rule_typeof(const RuleTree *rt, Rule rule) {
    return Rule_get(rt, rule)->type;
}

Rule Rule_by_name(const RuleTree *rt, const Word *name) {
    return (Rule){ IdMap_get(&rt->by_name, name) };
}

const RuleEntry *Rule_get(const RuleTree *rt, Rule rule) {
#ifdef DEBUG
    assert(rt->crystallized);
#endif

    return rt->entries.data[rule.id];
}

#define INDENT 2

static void dump_children(const RuleTree *rt, const Vec *children,
                          const RuleNode *exclude, int level) {
    // print matches
    for (size_t i = 0; i < children->len; ++i) {
        const RuleNode *child = children->data[i];

        if (child == exclude)
            continue;

        // print predicate
        printf("%*s", level * INDENT, "");

        const MatchAtom *pred = child->pred;

        switch (pred->type) {
        case MATCH_EXPR:
            if (pred->repeating)
                printf("repeating ");

            printf(TC_BLUE);
            TypeExpr_print(pred->rule_expr);
            printf(TC_RESET);
            break;
        case MATCH_LEXEME:
            printf("`" TC_GREEN "%.*s" TC_RESET,
                   (int)pred->lxm->len, pred->lxm->str);
            break;
        }

        // rule (if exists)
        if (child->has_rule) {
            const Word *name = Rule_get(rt, child->rule)->name;

            printf(TC_DIM " -> " TC_RED "%.*s" TC_RESET,
                   (int)name->len, name->str);
        }

        puts("");

        dump_children(rt, &child->nexts, child, level + 1);
    }
}

void RuleTree_dump(const RuleTree *rt) {
#ifdef DEBUG
    assert(rt->crystallized);
#endif

    puts(TC_CYAN "RuleTree:" TC_RESET);
    dump_children(rt, &rt->roots, NULL, 0);
    puts("");
}
