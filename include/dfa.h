#pragma once
#ifndef DFA_H
#define DFA_H

#include <stdbool.h>
#include <stddef.h>
#include "nfa.h"
#include "ast.h"

typedef struct {
    size_t id;
    bool is_accept;
} DFAState;

typedef struct {
    DFAState *states;
    size_t state_count;
    size_t state_capacity;
    size_t start_state;
    size_t *accept_states;
    size_t accept_count;
    struct {
        size_t from;
        char symbol;
        size_t to;
    } *transitions;
    size_t transition_count;
    size_t transition_capacity;
} DFA;

DFA* dfa_new(void);
void dfa_free(DFA *dfa);
DFA* dfa_from_nfa(NFA *nfa);
DFA* dfa_minimize(DFA *dfa);
void dfa_print_transition_table(DFA *dfa);
char* dfa_to_dot(DFA *dfa);

#endif
