#include <assert.h>

#include "rules.h"

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

// places RuleEntry in various data structures, does NOT update node tree (this
// is done in Rule_define)
static Rule register_entry(RuleTree *rt, RuleEntry *entry) {
    Rule handle = { rt->entries.len };

    Vec_push(&rt->entries, entry);
    IdMap_put(&rt->by_name, entry->name, 0);
    entry->type = Type_define(&rt->types, &(TypeDef){ .name = *entry->name });

    return handle;
}

RuleTree RuleTree_new(void) {
    RuleTree rt = {
        .pool = Bump_new(),
        .entries = Vec_new(),
        .by_name = IdMap_new(),
        .types = TypeGraph_new(),
    };

    rt.roots = Vec_new();

    // Scope rule
    RuleEntry *scope_entry = RT_alloc(&rt, sizeof(*scope_entry));
    Word name = WORD("Scope");

    *scope_entry = (RuleEntry){
        .name = Word_copy_of(&name, &rt.pool)
    };

    rt.rule_scope = register_entry(&rt, scope_entry);
    rt.ty_scope = scope_entry->type;

    // special types
    TypeGraph *types = &rt.types;

    rt.ty_any = types->ty_any;
    rt.ty_literal = Type_define(types, &(TypeDef){ .name = WORD("Literal") });
    rt.ty_lexeme = Type_define(types, &(TypeDef){ .name = WORD("Lexeme") });
    rt.ty_ident = Type_define(types, &(TypeDef){ .name = WORD("Ident") });

    return rt;
}

static void RuleNode_del(RuleNode *node) {
    for (size_t i = 0; i < node->nexts.len; ++i)
        RuleNode_del(node->nexts.data[i]);

    Vec_del(&node->nexts);
}

void RuleTree_del(RuleTree *rt) {
    for (size_t i = 0; i < rt->roots.len; ++i)
        RuleNode_del(rt->roots.data[i]);

    TypeGraph_del(&rt->types);
    IdMap_del(&rt->by_name);
    Vec_del(&rt->entries);
    Bump_del(&rt->pool);
}

Rule Rule_define(RuleTree *rt, RuleDef *def) {
    assert(def->pat.len > 0);

    // create entry
    RuleEntry *entry = RT_alloc(rt, sizeof(*entry));

    *entry = (RuleEntry){
        .name = Word_copy_of(&def->name, &rt->pool),
        .pat = def->pat,
        .prec = def->prec
    };

    Rule handle = register_entry(rt, entry);

    // place entry
    Pattern *pat = &def->pat;
    RuleNode *node;
    Vec *nexts = &rt->roots;

    for (size_t i = 0; i < pat->len; ++i) {
        MatchAtom *pred = &pat->matches[i];
        node = NULL;

        // check for equivalent predicate
        for (size_t j = 0; j < nexts->len; ++j) {
            RuleNode *next = nexts->data[j];
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
                node = next;
                break;
            }
        }

        // no match found, create a new node
        if (!node) {
            node = RT_new_node(rt, pred);
            Vec_push(nexts, node); // TODO should I copy predicate here?
        }

        nexts = &node->nexts;
    }

    // check if valid, if so place and return
    if (node->has_rule)
        fungus_panic("duplicate rules"); // TODO don't panic

    node->rule = handle;
    node->has_rule = true;

    return handle;
}

Rule Rule_by_name(const RuleTree *rt, const Word *name) {
    return (Rule){ IdMap_get(&rt->by_name, name) };
}

Type Rule_typeof(const RuleTree *rt, Rule rule) {
    return Rule_get(rt, rule)->type;
}

const RuleEntry *Rule_get(const RuleTree *rt, Rule rule) {
    return rt->entries.data[rule.id];
}

#define INDENT 2

static void dump_children(const RuleTree *rt, const Vec *children, int level) {
    // print matches
    for (size_t i = 0; i < children->len; ++i) {
        RuleNode *child = children->data[i];

        // next type
        MatchAtom *pred = child->pred;

        printf("%*s", level * INDENT, "");

        switch (pred->type) {
        case MATCH_LEXEME:
            printf("%.*s", (int)pred->lxm->len, pred->lxm->str);
            break;
        case MATCH_EXPR:
            printf(TC_BLUE);
            TypeExpr_print(&rt->types, pred->rule_expr);
            printf(TC_RESET);
            break;
        }

        // rule (if exists)
        if (child->has_rule) {
            const Word *name = Rule_get(rt, child->rule)->name;

            printf(TC_DIM " -> " TC_RED "%.*s" TC_RESET,
                   (int)name->len, name->str);
        }

        puts("");

        dump_children(rt, &child->nexts, level + 1);
    }
}

void RuleTree_dump(const RuleTree *rt) {
    puts(TC_CYAN "RuleTree:" TC_RESET);
    dump_children(rt, &rt->roots, 0);
    puts("");
}
