#ifndef PRECEDENCE_H
#define PRECEDENCE_H

/*
 * precedence is represented by a directed acyclic graph, allowing for intuitive
 * and symbolic runtime definition of precedences.
 */

#include "../data.h"

typedef struct PrecHandle { unsigned id; } Prec;
typedef enum Associativity { ASSOC_LEFT, ASSOC_RIGHT } Associativity;

/*
 * TODO Precs_crystallize, flattening graph by assigning a number to each
 * precedence for O(1) comparison
 */
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

void Precs_dump(const Precs *);

#endif
