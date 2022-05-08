#include "precedence.h"

#define PRECS_INIT_CAP 16

Precs Precs_new(void) {
    return (Precs){
        .pool = Bump_new(),
        .by_name = IdMap_new(),

        .names = malloc(sizeof(const Word *) * PRECS_INIT_CAP),
        .assocs = malloc(sizeof(Associativity) * PRECS_INIT_CAP),
        .cap = PRECS_INIT_CAP
    };
}

void Precs_del(Precs *p) {
    free(p->names);
    free(p->assocs);
    IdMap_del(&p->by_name);
    Bump_del(&p->pool);
}

Prec Prec_define(Precs *p, Word name, Associativity assoc) {
    if (p->len == p->cap) {
        p->cap *= 2;
        p->names = realloc(p->names, sizeof(*p->names) * p->cap);
        p->assocs = realloc(p->assocs, sizeof(*p->assocs) * p->cap);
    }

    p->names[p->len] = Word_copy_of(&name, &p->pool);
    p->assocs[p->len] = assoc;

    Prec prec = { p->len++ };

    IdMap_put(&p->by_name, p->names[prec.id], prec.id);

    return prec;
}

bool Prec_by_name_checked(Precs *p, const Word *name, Prec *o_prec) {
    unsigned id;

    if (IdMap_get_checked(&p->by_name, name, &id)) {
        o_prec->id = id;

        return true;
    }

    return false;
}

Prec Prec_by_name(Precs *p, const Word *name) {
    Prec res;

    if (Prec_by_name_checked(p, name, &res))
        return res;

    fungus_panic("failed to retrieve precedence %.*s.",
                 (int)name->len, name->str);
}

int Prec_cmp(Prec a, Prec b) {
    return a.id - b.id;
}

Associativity Prec_assoc(const Precs *p, Prec prec) {
    return p->assocs[prec.id];
}

void Precs_dump(const Precs *p) {
    puts(TC_CYAN "Precs:" TC_RESET);

    for (size_t i = 0; i < p->len; ++i) {
        const Word *name = p->names[i];

        printf("%.*s <%s>\n", (int)name->len, name->str,
               p->assocs[i] == ASSOC_LEFT ? "L" : "R");
    }

    puts("");
}
