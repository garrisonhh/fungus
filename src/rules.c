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

    return node;
}

RuleTree RuleTree_new(void) {
    RuleTree rt = {
        .pool = Bump_new()
    };

    rt.root = RT_new_node(&rt, NULL, INVALID_TYPE);

    return rt;
}

void RuleTree_del(RuleTree *rt) {
    // TODO vecs from rulenodes are leaked

    Bump_del(&rt->pool);
}

Rule *Rule_define(Fungus *fun, RuleDef *def) {
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
    Rule *rule = RT_alloc(rt, sizeof(*rule));

    *rule = (Rule){
        .name = Word_copy_of(&def->name, &fun->rules.pool),
        .prec = def->prec,
        .assoc = def->assoc,
        .cty = def->cty,
        .hook = def->hook,

        .prefixed = Type_is(types, def->pattern[0].ty, fun->t_lexeme),
        .postfixed = Type_is(types, def->pattern[def->len - 1].ty,
                             fun->t_lexeme),
    };

    // place node
    Vec *next_rtnodes = &rt->root->nexts;
    RuleNode *cur_rtnode = NULL;

    for (size_t i = 0; i < def->len; ++i) {
        PatNode *cur_patnode = &def->pattern[i];

        for (size_t j = 0; j < next_rtnodes->len; ++j) {
            RuleNode *pot_match = next_rtnodes->data[j];

            if (cur_patnode->ty.id == pot_match->ty.id) {
                // perfect match
                cur_rtnode = pot_match;
                goto next_node;
            } else if (Type_is(&fun->types, cur_patnode->ty, pot_match->ty)) {
                // supertype match, put this node in front of it so that rule
                // checking sees the most specific type first
                cur_rtnode = RT_new_node(rt, cur_rtnode, cur_patnode->ty);
                Vec_ordered_insert(next_rtnodes, j, cur_rtnode);
                goto next_node;
            }
        }

        // no node match found, create new node
        cur_rtnode = RT_new_node(rt, cur_rtnode, cur_patnode->ty);
        Vec_push(next_rtnodes, cur_rtnode);

next_node:
        // iterate
        next_rtnodes = &cur_rtnode->nexts;

        // allow repetition
        if (cur_patnode->modifiers & PAT_REPEAT)
            Vec_push(&cur_rtnode->nexts, cur_rtnode);
    }

    // add rule to last node and return
    return cur_rtnode->rule = rule;
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
    if (node->rule) {
        name = node->rule->name;

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
