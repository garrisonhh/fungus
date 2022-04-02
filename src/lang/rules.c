#include <assert.h>

#include "rules.h"

static void *RT_alloc(RuleTree *rt, size_t n_bytes) {
    return Bump_alloc(&rt->pool, n_bytes);
}

static RuleNode *RT_new_node(RuleTree *rt) {
    RuleNode *node = RT_alloc(rt, sizeof(*node));

    *node = (RuleNode){0};

    return node;
}

RuleTree RuleTree_new(void) {
    RuleTree rt = {
        .pool = Bump_new(),
        .entries = Vec_new(),
    };

    rt.root = RT_new_node(&rt);

    return rt;
}

void RuleTree_del(RuleTree *rt) {
    Vec_del(&rt->entries);
    Bump_del(&rt->pool);
}

Rule Rule_define(RuleTree *rt, RuleDef *def) {
    assert(def->pat.len > 0);

    // create entry
    Rule handle = { rt->entries.len + 1 }; // handle ids start at 1
    RuleEntry *entry = RT_alloc(rt, sizeof(*entry));

    *entry = (RuleEntry){
        .name = Word_copy_of(&def->name, &rt->pool),
        .pat = def->pat,
        .prec = def->prec,

        .prefixed = !def->pat.is_expr[0],
        .postfixed = !def->pat.is_expr[def->pat.len - 1],
    };

    Vec_push(&rt->entries, entry);

    // place entry
    Pattern *pat = &def->pat;
    RuleNode *trav = rt->root;

    for (size_t i = 0; i < pat->len; ++i) {
        if (pat->is_expr[i]) {
            // expr node
            if (!trav->next_expr)
                trav->next_expr = RT_new_node(rt);

            trav = trav->next_expr;
        } else {
            // lexeme node
            RuleNode *next = NULL;

            for (size_t j = 0; j < trav->num_next_lxms; ++j) {
                if (Word_eq(pat->pat[i], trav->next_lxm_type[j])) {
                    next = trav->next_lxm[i];
                    break;
                }
            }

            if (!next) {
                next = trav->next_lxm[trav->num_next_lxms] = RT_new_node(rt);
                trav->next_lxm_type[trav->num_next_lxms] = pat->pat[i];
                ++trav->num_next_lxms;
            }

            trav = next;
        }
    }

    // place rule
    if (Rule_is_valid(trav->rule)) {
        fungus_error("duplicate rules:");

#define PRINT_RULE(PAT, NAME) {\
    Pattern_print(PAT);\
    printf(" -> %.*s\n", (int)(NAME)->len, (NAME)->str);\
}
        const RuleEntry *entry = Rule_get(rt, trav->rule);

        PRINT_RULE(&entry->pat, entry->name);
        PRINT_RULE(pat, &def->name);

#undef PRINT_RULE

        // TODO don't panic
        abort();
    }

    trav->rule = handle;

    return handle;
}

bool Rule_is_valid(Rule rule) {
    return rule.id > 0;
}

const RuleEntry *Rule_get(const RuleTree *rt, Rule rule) {
    assert(Rule_is_valid(rule));

    return rt->entries.data[rule.id - 1];
}

#define INDENT 2

static void RuleNode_dump(const RuleTree *rt, RuleNode *node, int level) {
    // print terminus name
    if (Rule_is_valid(node->rule)) {
        const Word *name = Rule_get(rt, node->rule)->name;

        printf(TC_RED " -> %.*s" TC_RESET,
               (int)name->len, name->str);
    }

    // print newline after everything but root
    if (level)
        puts("");

    // print nexts
    if (node->next_expr) {
        printf("%*s" TC_BLUE "expr" TC_RESET, INDENT * level, "");

        RuleNode_dump(rt, node->next_expr, level + 1);
    }

    for (size_t i = 0; i < node->num_next_lxms; ++i) {
        const Word *name = node->next_lxm_type[i];

        printf("%*s%.*s", INDENT * level, "", (int)name->len, name->str);

        RuleNode_dump(rt, node->next_lxm[i], level + 1);
    }
}

void RuleTree_dump(const RuleTree *rt) {
    puts(TC_CYAN "RuleTree:" TC_RESET);
    RuleNode_dump(rt, rt->root, 0);
    puts("");
}
