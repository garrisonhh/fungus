#include "rules.h"

RuleTree RuleTree_new(void) {
    return (RuleTree){
        .pool = Bump_new(),
        .roots = Vec_new()
    };
}

void RuleTree_del(RuleTree *rt) {
    Vec_del(&rt->roots);
    Bump_del(&rt->pool);
}

static void *RT_alloc(RuleTree *rt, size_t nbytes) {
    return Bump_alloc(&rt->pool, nbytes);
}

static RTNode *RT_new_node(RuleTree *rt, RuleAtom *atom) {
    RTNode *node = RT_alloc(rt, sizeof(*node));

    *node = (RTNode){
        .nexts = Vec_new(),
        .atom = *atom
    };

    return node;
}

static bool RuleAtom_eq(RuleAtom *a, RuleAtom *b) {
    return a->ty.id == b->ty.id
           && a->mty.id == b->mty.id
           && a->modifiers == b->modifiers;
}

void Rule_define(RuleTree *rt, RuleDef *def) {
    // validate RuleDef
    bool valid = def->len && def->pattern;

    if (!valid) // TODO better error handling
        fungus_panic("invalid rule definition.");

    // create Rule
    Rule *rule = RT_alloc(rt, sizeof(*rule));

    *rule = (Rule){ def->prec, def->ty, def->mty, def->interpret };

    // place node
    Vec *next_atoms = &rt->roots;
    RTNode *cur_node;

    for (size_t i = 0; i < def->len; ++i) {
        RuleAtom *cur_atom = &def->pattern[i];

        for (size_t j = 0; j < next_atoms->len; ++j) {
            RTNode *maybe_match = next_atoms->data[j];

            if (RuleAtom_eq(cur_atom, &maybe_match->atom)) {
                cur_node = maybe_match;
                goto next_atom;
            }
        }

        // no node match found, create new node
        cur_node = RT_new_node(rt, cur_atom);
        Vec_push(next_atoms, cur_node);

        // iterate
next_atom:
        next_atoms = &cur_node->nexts;
    }

    // add rule to last node
    cur_node->rule = rule;
}

/* TODO
static void RTNode_dump(RTNode *node, char *buf, char *tail);

static void RTNode_nexts_dump(Vec *nexts, char *buf, char *tail) {
    for (size_t i = 0; i < nexts->len; ++i)
        RTNode_dump(nexts->data[i], buf, tail);
}

static void RTNode_dump(RTNode *node, char *buf, char *tail) {
    // sprintf atom pattern
    tail += sprintf(tail, "(");

    if (node->atom.ty.id != TY_NONE) {
        TypeEntry *entry = type_get(node->atom.ty);

        tail += sprintf(tail, "%.*s", (int)entry->name->len, entry->name->str);
    } else {
        tail += sprintf(tail, "_");
    }

    tail += sprintf(tail, " ");

    if (node->atom.mty.id != MTY_NONE) {
        TypeEntry *entry = metatype_get(node->atom.mty);

        tail += sprintf(tail, "%.*s", (int)entry->name->len, entry->name->str);
    } else {
        tail += sprintf(tail, "_");
    }

    if (node->atom.modifiers) {
        tail += sprintf(tail, " ");

        if (node->atom.modifiers & RAM_REPEAT)
            tail += sprintf(tail, "*");
        if (node->atom.modifiers & RAM_OPTIONAL)
            tail += sprintf(tail, "?");
    }

    tail += sprintf(tail, ") ");

    // print rule if exists
    if (node->rule) {
        printf("  %s-> ", buf);

        TypeEntry *rule_entry = metatype_get(node->rule->mty);

        printf("%.*s\n", (int)rule_entry->name->len, rule_entry->name->str);
    }

    // recur on children
    RTNode_nexts_dump(&node->nexts, buf, tail);
}
*/

void RuleTree_dump(RuleTree *rt) {
    // char buf[1024];

    puts("rules:");
    // RTNode_nexts_dump(&rt->roots, buf, buf);
    puts("TODO redo");
    puts("");
}
