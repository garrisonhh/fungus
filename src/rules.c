#include "rules.h"
#include "fungus.h"

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

static void *RT_alloc(RuleTree *rt, size_t n_bytes) {
    return Bump_alloc(&rt->pool, n_bytes);
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
    return a->mty.id == b->mty.id && a->modifiers == b->modifiers;
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

static void RTNode_dump(Fungus *fun, RTNode *node, char *buf, char *tail);

static void RTNode_nexts_dump(Fungus *fun, Vec *nexts, char *buf, char *tail) {
    for (size_t i = 0; i < nexts->len; ++i)
        RTNode_dump(fun, nexts->data[i], buf, tail);
}

static void RTNode_dump(Fungus *fun, RTNode *node, char *buf, char *tail) {
    const Word *ty_name = Type_name(&fun->types, node->atom.mty);

    tail += sprintf(tail, "%.*s", (int)ty_name->len, ty_name->str);

    if (node->atom.modifiers) {
        if (node->atom.modifiers & RAM_REPEAT)
            tail += sprintf(tail, "*");
        if (node->atom.modifiers & RAM_OPTIONAL)
            tail += sprintf(tail, "?");
    }

    tail += sprintf(tail, " ");

    // print rule if exists
    if (node->rule) {
        printf("%s-> ", buf);
        Fungus_print_types(fun, node->rule->ty, node->rule->mty);
        puts("");
    }

    // recur on children
    RTNode_nexts_dump(fun, &node->nexts, buf, tail);
}

void RuleTree_dump(Fungus *fun, RuleTree *rt) {
    char buf[1024];

    term_format(TERM_CYAN);
    puts("RuleTree:");
    term_format(TERM_RESET);

    RTNode_nexts_dump(fun, &rt->roots, buf, buf);
    puts("");
}
