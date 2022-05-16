#ifndef PRECEDENCE_H
#define PRECEDENCE_H

/*
 * precedence is represented by a flat array, since languages will provide
 * all precedences up front this is a very efficient representation
 */

#include "../data.h"

typedef struct PrecHandle { unsigned id; } Prec;
typedef enum Associativity { ASSOC_LEFT, ASSOC_RIGHT } Associativity;

typedef struct Precs {
    Bump pool;
    IdMap by_name;

    const Word **names;
    Associativity *assocs;
    size_t len, cap;
} Precs;

typedef struct PrecDef {
    Word name;
    Associativity assoc;
} PrecDef;

Precs Precs_new(void);
void Precs_del(Precs *);

Prec Prec_define(Precs *, Word name, Associativity assoc);

bool Prec_by_name_checked(Precs *, const Word *name, Prec *o_prec);
Prec Prec_by_name(Precs *, const Word *name);
int Prec_cmp(Prec a, Prec b);
Associativity Prec_assoc(const Precs *, Prec prec);

// for parser iteration
static inline Prec Prec_highest(const Precs *p)
    { return (Prec){ (unsigned)p->len - 1 }; }
static inline void Prec_dec(Prec *prec) { --prec->id; };
static inline bool Prec_is_lowest(Prec prec) { return prec.id == 0; };

void Precs_dump(const Precs *);

#endif
