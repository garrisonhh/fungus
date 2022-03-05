#include "rules.h"
#include "fungus.h"

static void *RT_alloc(RuleTree *rt, size_t n_bytes) {
    return Bump_alloc(&rt->pool, n_bytes);
}

static RuleNode *RT_new_node(RuleTree *rt, RuleNode *parent, Type ty) {
    RuleNode *node = RT_alloc(rt, sizeof(*node));

    *node = (RuleNode){
        .ty = ty,
        .nexts = Vec_new(),
        .parent = parent
    };

    if (parent)
        Vec_push(&parent->nexts, node);

    return node;
}

RuleTree RuleTree_new(void) {
    RuleTree rt = {
        .pool = Bump_new(),
        .entries = Vec_new(),
    };

    rt.root = RT_new_node(&rt, NULL, INVALID_TYPE);

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

static void RuleNode_place(RuleTree *rt, RuleNode *node, PatNode *pat,
                           size_t len, Rule rule) {
    if (len == 0) {
        // reached end of pattern
        node->terminates = true;
        node->rule = rule;

        return;
    }

    // optional modifier
    if (pat->modifiers & PAT_OPTIONAL)
        RuleNode_place(rt, node, pat + 1, len - 1, rule);

    // try to match a node
    RuleNode *placed = NULL;

    for (size_t i = 0; i < node->nexts.len; ++i) {
        RuleNode *next = node->nexts.data[i];

        if (next->ty.id == pat->ty.id) {
            placed = next;
            break;
        }
    }

    if (!placed) // no match, create a new node
        placed = RT_new_node(rt, node, pat->ty);

    // repeat modifier
    if (pat->modifiers & PAT_REPEAT) {
        bool already_repeats = false;

        for (size_t i = 0; i < placed->nexts.len; ++i) {
            if (placed->nexts.data[i] == placed) {
                already_repeats = true;
                break;
            }
        }

        if (!already_repeats)
            Vec_push(&placed->nexts, placed);
    }

    // recur on next pattern
    RuleNode_place(rt, placed, pat + 1, len - 1, rule);
}

Rule Rule_define(Fungus *fun, RuleDef *def) {
    RuleTree *rt = &fun->rules;
    TypeGraph *types = &fun->types;

    // validate RuleDef
    bool valid = def->len && def->len < MAX_RULE_LEN && def->pattern;

    if (!valid) {
        // TODO more intelligent errors for this
        fungus_error("invalid rule definition: %.*s",
                     (int)def->name.len, def->name.str);
    }

    // create Rule
    RuleEntry *entry = RT_alloc(rt, sizeof(*entry));
    Rule handle = { rt->entries.len };

    *entry = (RuleEntry){
        .handle = handle,
        .name = Word_copy_of(&def->name, &fun->rules.pool),
        .prec = def->prec,
        .assoc = def->assoc,
        .cty = def->cty,
        .hook = def->hook,

        .prefixed = Type_is(types, def->pattern[0].ty, fun->t_lexeme),
        .postfixed = Type_is(types, def->pattern[def->len - 1].ty,
                             fun->t_lexeme),
    };

    Vec_push(&rt->entries, entry);

    // place node entry in tree
    RuleNode_place(rt, rt->root, def->pattern, def->len, handle);

    return handle;
}

RuleEntry *Rule_get(RuleTree *rt, Rule rule) {
    return rt->entries.data[rule.id];
}

static void RuleNode_dump(Fungus *fun, RuleNode *node, char *buf, char *tail) {
    const Word *name = Type_name(&fun->types, node->ty);

    tail += sprintf(tail, "%.*s", (int)name->len, name->str);

    // repeating patterns
    for (size_t i = 0; i < node->nexts.len; ++i) {
        if (node->nexts.data[i] == node) {
            tail += sprintf(tail, "[*]");
            break;
        }
    }

    tail += sprintf(tail, " ");

    // print rule if exists
    if (node->terminates) {
        name = Rule_get(&fun->rules, node->rule)->name;

        printf("%.*s: %s\n", (int)name->len, name->str, buf);
    }

    // recur on children
    for (size_t i = 0; i < node->nexts.len; ++i)
        if (node->nexts.data[i] != node)
            RuleNode_dump(fun, node->nexts.data[i], buf, tail);
}

void RuleTree_dump(Fungus *fun, RuleTree *rt) {
    char buf[1024];

    term_format(TERM_CYAN);
    puts("RuleTree:");
    term_format(TERM_RESET);

    for (size_t i = 0; i < rt->root->nexts.len; ++i)
        RuleNode_dump(fun, rt->root->nexts.data[i], buf, buf);

    puts("");
}
