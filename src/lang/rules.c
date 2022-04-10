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
        .types = TypeGraph_new(),
        .backtracks = Vec_new(),
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

    Vec_del(&rt->backtracks);
    TypeGraph_del(&rt->types);
    IdMap_del(&rt->by_name);
    Vec_del(&rt->entries);
    Bump_del(&rt->pool);
}

static void place_rule(RuleTree *rt, const Pattern *pat, Rule rule) {
    assert(pat->len > 0);

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

    node->rule = rule;
    node->has_rule = true;
}

Type Rule_immediate_type(RuleTree *rt, Word name) {
    return Type_define(&rt->types, &(TypeDef){ .name = name });
}

Rule Rule_immediate_define(RuleTree *rt, Type type, Prec prec, Pattern pat) {
    assert(Type_is_in(&rt->types, type));

    RuleEntry *entry = RT_alloc(rt, sizeof(*entry));

    *entry = (RuleEntry){
        .name = Word_copy_of(Type_get(&rt->types, type)->name, &rt->pool),
        .pat = pat,
        .prec = prec,
        .type = type
    };

    Rule handle = register_entry(rt, entry);

    place_rule(rt, &pat, handle);

    return handle;
}

Rule Rule_define(RuleTree *rt, Word name, Prec prec, AstExpr *pat_ast) {
    UNIMPLEMENTED;
}

void RuleTree_crystallize(RuleTree *rt) {
    // TODO compile patterns

    // generate backtrack parallel vec
    for (size_t i = 0; i < rt->roots.len; ++i) {
        const RuleNode *root = rt->roots.data[i];

        RuleBacktrack *bt = RT_alloc(rt, sizeof(*bt));

        *bt = (RuleBacktrack){
            .pred = root->pred,
            .backs = Vec_new()
        };

        Vec_push(&rt->backtracks, bt);
    }

    // generate backtracks
    for (size_t i = 0; i < rt->roots.len; ++i) {
        const RuleNode *root = rt->roots.data[i];

        for (size_t j = 0; j < root->nexts.len; ++j) {
            const RuleNode *next = root->nexts.data[j];

            // check if next is a root
            bool is_root = false;

            for (size_t k = 0; k < rt->roots.len; ++k) {
                if (k == i)
                    continue;

                const RuleNode *other_root = rt->roots.data[k];

                if (MatchAtom_equals(next->pred, other_root->pred)) {
                    is_root = true;
                    break;
                }
            }

            // if next is a root, add root as a backtrack for next
            if (is_root) {
                RuleBacktrack *bt = rt->backtracks.data[j];

                Vec_push(&bt->backs, root);
            }
        }
    }

#ifdef DEBUG
    rt->crystallized = true;
#endif
}

Type Rule_typeof(const RuleTree *rt, Rule rule) {
    return Rule_get(rt, rule)->type;
}

Rule Rule_by_name(const RuleTree *rt, const Word *name) {
#ifdef DEBUG
    assert(rt->crystallized);
#endif

    return (Rule){ IdMap_get(&rt->by_name, name) };
}

const RuleEntry *Rule_get(const RuleTree *rt, Rule rule) {
#ifdef DEBUG
    assert(rt->crystallized);
#endif

    return rt->entries.data[rule.id];
}

#define INDENT 2

static void dump_children(const RuleTree *rt, const Vec *children, int level) {
    // print matches
    for (size_t i = 0; i < children->len; ++i) {
        RuleNode *child = children->data[i];

        printf("%*s", level * INDENT, "");
        MatchAtom_print(child->pred, &rt->types, NULL);

        // rule (if exists)
        if (child->has_rule) {
            const Word *name = Rule_get(rt, child->rule)->name;

            printf(TC_DIM " -> " TC_RED "%.*s" TC_RESET,
                   (int)name->len, name->str);
        }

        puts("");

        dump_children(rt, &child->nexts, level + 1);

        if (level == 0 && rt->backtracks.len > 0) {
            // print backtracks
            const RuleBacktrack *bt = rt->backtracks.data[i];

            for (size_t j = 0; j < bt->backs.len; ++j) {
                printf(TC_DIM " <- " TC_RESET);
                MatchAtom_print(((RuleNode *)bt->backs.data[j])->pred,
                                &rt->types, NULL);
                puts("");
            }
        }
    }
}

void RuleTree_dump(const RuleTree *rt) {
    puts(TC_CYAN "RuleTree:" TC_RESET);
    dump_children(rt, &rt->roots, 0);

    puts("");
}
