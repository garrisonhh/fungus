#include <assert.h>

#include "rules.h"

static void *RT_alloc(RuleTree *rt, size_t n_bytes) {
    return Bump_alloc(&rt->pool, n_bytes);
}

static RuleNode *RT_new_node(RuleTree *rt) {
    RuleNode *node = RT_alloc(rt, sizeof(*node));

    *node = (RuleNode){
        .next_atoms = Vec_new(),
        .next_nodes = Vec_new()
    };

    return node;
}

// places RuleEntry in various data structures, does NOT update node tree (this
// is done in Rule_define)
static Rule register_entry(RuleTree *rt, RuleEntry *entry) {
    Rule handle = { rt->entries.len };

    Vec_push(&rt->entries, entry);
    IdMap_put(&rt->by_name, entry->name, 0);
    entry->ty = Type_define(&rt->types, &(TypeDef){ .name = *entry->name });

    return handle;
}

RuleTree RuleTree_new(void) {
    RuleTree rt = {
        .pool = Bump_new(),
        .entries = Vec_new(),
        .by_name = IdMap_new(),
        .types = TypeGraph_new(),
    };

    rt.root = RT_new_node(&rt);

    // Scope rule
    RuleEntry *scope_entry = RT_alloc(&rt, sizeof(*scope_entry));
    Word name = WORD("Scope");

    *scope_entry = (RuleEntry){
        .name = Word_copy_of(&name, &rt.pool)
    };

    register_entry(&rt, scope_entry);

    rt.ty_scope = scope_entry->ty;

    return rt;
}

static void RuleNode_del(RuleNode *node) {
    for (size_t i = 0; i < node->next_nodes.len; ++i)
        RuleNode_del(node->next_nodes.data[i]);

    Vec_del(&node->next_nodes);
    Vec_del(&node->next_atoms);
}

void RuleTree_del(RuleTree *rt) {
    RuleNode_del(rt->root);

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
    RuleNode *trav = rt->root;

    for (size_t i = 0; i < pat->len; ++i) {
        MatchAtom *atom = &pat->matches[i];
        RuleNode *next = NULL;

        // check for equivalent MatchAtom
        for (size_t j = 0; j < trav->next_atoms.len; ++j) {
            MatchAtom *next_atom = trav->next_atoms.data[j];
            bool matching = false;

            if (atom->type == next_atom->type) {
                switch (atom->type) {
                case MATCH_LEXEME:
                    matching = Word_eq(atom->lxm, next_atom->lxm);
                    break;
                case MATCH_EXPR:
                    matching =
                        TypeExpr_equals(atom->rule_expr, next_atom->rule_expr);
                    break;
                }
            }

            if (matching) {
                next = trav->next_nodes.data[j];
                break;
            }
        }

        // no match found, create a new node
        if (!next) {
            next = RT_new_node(rt);
            Vec_push(&trav->next_nodes, next);
            Vec_push(&trav->next_atoms, atom); // TODO should I copy here?
        }

        trav = next;
    }

    // check if valid, if so place and return
    if (trav->has_rule)
        fungus_panic("duplicate rules"); // TODO don't panic

    trav->rule = handle;
    trav->has_rule = true;

    return handle;
}

Rule Rule_by_name(const RuleTree *rt, const Word *name) {
    return (Rule){ IdMap_get(&rt->by_name, name) };
}

const RuleEntry *Rule_get(const RuleTree *rt, Rule rule) {
    return rt->entries.data[rule.id];
}

#define INDENT 2

static void dump_children(const RuleTree *rt, RuleNode *node, int level) {
    // print matches
    for (size_t i = 0; i < node->next_atoms.len; ++i) {
        MatchAtom *atom = node->next_atoms.data[i];

        printf("%*s", level * INDENT, "");

        switch (atom->type) {
        case MATCH_LEXEME:
            printf("%.*s", (int)atom->lxm->len, atom->lxm->str);
            break;
        case MATCH_EXPR:
            TypeExpr_print(&rt->types, atom->rule_expr);
            break;
        }

        puts("");

        dump_children(rt, node->next_nodes.data[i], level + 1);
    }
}

void RuleTree_dump(const RuleTree *rt) {
    puts(TC_CYAN "RuleTree:" TC_RESET);
    dump_children(rt, rt->root, 0);
    puts("");
}
