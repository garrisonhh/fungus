#include <assert.h>

#include "rules.h"
#include "fungus.h"

static void *RT_alloc(RuleTree *rt, size_t n_bytes) {
    return Bump_alloc(&rt->pool, n_bytes);
}

static RuleNode *RT_new_node(RuleTree *rt, RuleNode *parent, TypeExpr *te) {
    RuleNode *node = RT_alloc(rt, sizeof(*node));

    *node = (RuleNode){
        .nexts = Vec_new(),
        .te = te, // te should be bump allocated in Rule_defin
    };

    if (parent)
        Vec_push(&parent->nexts, node);

    return node;
}

static Pattern Pattern_copy_of(RuleTree *rt, Pattern *pat) {
    // allocate
    Pattern copy = {
        .pat = RT_alloc(rt, pat->len * sizeof(*copy.pat)),
        .len = pat->len,
        .returns = pat->returns,
        .where = RT_alloc(rt, pat->where_len * sizeof(*copy.where)),
        .where_len = pat->where_len
    };

    for (size_t i = 0; i < pat->len; ++i)
        copy.pat[i] = pat->pat[i];

    for (size_t i = 0; i < pat->where_len; ++i)
        copy.where[i] = TypeExpr_deepcopy(&rt->pool, pat->where[i]);

    return copy;
}

RuleTree RuleTree_new(void) {
    RuleTree rt = {
        .pool = Bump_new(),
        .entries = Vec_new(),
    };

    rt.root = RT_new_node(&rt, NULL, NULL);

    return rt;
}

static void RuleNode_del(RuleNode *node) {
    for (size_t i = 0; i < node->nexts.len; ++i)
        RuleNode_del(node->nexts.data[i]);

    Vec_del(&node->nexts);
}

void RuleTree_del(RuleTree *rt) {
    RuleNode_del(rt->root);
    Vec_del(&rt->entries);
    Bump_del(&rt->pool);
}

// idx is into def->pat pattern
static void RuleNode_place(Fungus *fun, Rule rule, Pattern *pat, size_t idx,
                           RuleNode *node) {
    if (idx == pat->len) {
        // terminus
        if (node->terminates) // TODO error
            fungus_panic("redefined rule");

        node->terminates = true;
        node->rule = rule;
    } else {
        RuleNode *next = NULL;
        TypeExpr *where = pat->where[pat->pat[idx]];

        // try to match an old node
        for (size_t i = 0; i < node->nexts.len; ++i) {
            RuleNode *other = node->nexts.data[i];

            if (TypeExpr_deepequals(where, other->te)) {
                next = other;
                break;
            }
        }

        if (!next) // no node found, make a new one
            next = RT_new_node(&fun->rules, node, where);

        // recur on node
        RuleNode_place(fun, rule, pat, idx + 1, next);
    }
}

static bool validate_def(Fungus *fun, RuleDef *def) {
    bool long_enough = def->pat.len > 0;

    // TODO check for concrete return type

    return long_enough;
}

Rule Rule_define(Fungus *fun, RuleDef *def) {
    RuleTree *rt = &fun->rules;
    TypeGraph *types = &fun->types;

    // validate RuleDef
    if (!validate_def(fun, def)) {
        fungus_panic("invalid rule definition: %.*s",
                     (int)def->name.len, def->name.str);
    }

    // create Rule
    Rule handle = { rt->entries.len };
    RuleEntry *entry = RT_alloc(rt, sizeof(*entry));

    *entry = (RuleEntry){
        .rule = handle,
        .ty = Type_define(types, &(TypeDef){
            .name = def->name,
            .is = &fun->t_syntax,
            .is_len = 1
        }),

        .pat = Pattern_copy_of(rt, &def->pat),
        .prec = def->prec,
        .assoc = def->assoc,

        .prefixed = TypeExpr_is(types, def->pat.where[def->pat.pat[0]],
                                fun->t_lexeme),
        .postfixed = TypeExpr_is(types,
                                 def->pat.where[def->pat.pat[def->pat.len - 1]],
                                 fun->t_lexeme),
    };

    Vec_push(&rt->entries, entry);

    // place node entry in tree
    RuleNode_place(fun, handle, &entry->pat, 0, rt->root);

    return handle;
}

RuleEntry *Rule_get(RuleTree *rt, Rule rule) {
    assert(rule.id < rt->entries.len);

    return rt->entries.data[rule.id];
}

static void RuleNode_dump(Fungus *fun, RuleNode *node, Vec *stack) {
    Vec_push(stack, node);

    // print rule if exists
    if (node->terminates) {
        // print nodes
        for (size_t i = 0; i < stack->len; ++i) {
            TypeExpr_print(&fun->types, ((RuleNode *)stack->data[i])->te);
            printf(" -> ");
        }

        // print rule name
        Type ty = Rule_get(&fun->rules, node->rule)->ty;
        const Word *name = Type_name(&fun->types, ty);

        printf("%.*s\n", (int)name->len, name->str);
    }

    // recur on children
    for (size_t i = 0; i < node->nexts.len; ++i)
        if (node->nexts.data[i] != node)
            RuleNode_dump(fun, node->nexts.data[i], stack);

    Vec_pop(stack);
}

void RuleTree_dump(Fungus *fun, RuleTree *rt) {
    term_format(TERM_CYAN);
    puts("RuleTree:");
    term_format(TERM_RESET);

    Vec buf = Vec_new();

    for (size_t i = 0; i < rt->root->nexts.len; ++i)
        RuleNode_dump(fun, rt->root->nexts.data[i], &buf);

    Vec_del(&buf);

    puts("");
}
