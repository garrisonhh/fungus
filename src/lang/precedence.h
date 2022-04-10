#ifndef PRECEDENCE_H
#define PRECEDENCE_H

/*
 * precedence is represented by a directed acyclic graph, allowing for intuitive
 * and symbolic runtime definition of precedences.
 *
 * TODO simplify this to a linked list type of deal, I think
 */

#include "../data.h"

typedef struct PrecHandle { unsigned id; } Prec;

#define MAX_PRECEDENCES 256

typedef enum Associativity { ASSOC_LEFT, ASSOC_RIGHT } Associativity;

typedef struct PrecEntry {
    const Word *name;

    // list of precedences this precedence is higher than
    // TODO impl IdSet here
    Prec above[MAX_PRECEDENCES];
    size_t above_len;

    Associativity assoc;
} PrecEntry;

/*
 * TODO PrecGraph_crystallize, flattening graph by assigning a number to each
 * precedence for O(1) comparison
 */
typedef struct PrecGraph {
    Bump pool;
    Vec entries; // entries indexed by id
    IdMap by_name;
} PrecGraph;

typedef struct PrecDef {
    Word name;
    Associativity assoc;
    Prec *above, *below;
    size_t above_len, below_len;
} PrecDef;

PrecGraph PrecGraph_new(void);
void PrecGraph_del(PrecGraph *);

Prec Prec_define(PrecGraph *, PrecDef *def);
bool Prec_by_name_checked(PrecGraph *, const Word *name, Prec *o_prec);
Prec Prec_by_name(PrecGraph *, const Word *name);
Comparison Prec_cmp(const PrecGraph *, Prec a, Prec b);
Associativity Prec_assoc(const PrecGraph *, Prec prec);

void PrecGraph_dump(const PrecGraph *);

#endif
