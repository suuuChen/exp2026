#ifndef NFA_H
#define NFA_H

#include <stdbool.h>
#include <stddef.h>
#include "ast.h"  // 包含 CharRange 定义

typedef struct {
    size_t id;
    bool is_accept;
} NFAState;

typedef enum {
    TRANS_EPSILON,
    TRANS_CHAR,
    TRANS_ANY_CHAR,
    TRANS_DIGIT,
    TRANS_WORD,
    TRANS_WHITESPACE,
    TRANS_CHAR_CLASS
} TransitionType;

typedef struct {
    TransitionType type;
    union {
        char ch;
        struct {
            bool negated;
            char *chars;
            size_t chars_len;
            CharRange *ranges;  // 使用 CharRange
            size_t ranges_len;
        } char_class;
    };
} NFATransition;

typedef struct {
    size_t from;
    NFATransition transition;
    size_t to;
} NFAEdge;

typedef struct {
    NFAState *states;
    size_t state_count;
    size_t state_capacity;
    NFAEdge *edges;
    size_t edge_count;
    size_t edge_capacity;
    size_t start_state;
    size_t accept_state;
    size_t *capture_group_states;
    size_t capture_group_count;
} NFA;

NFA* nfa_new(void);
void nfa_free(NFA *nfa);
NFA* nfa_from_ast(AstNode *ast);
void nfa_add_state(NFA *nfa, bool is_accept);
void nfa_add_edge(NFA *nfa, size_t from, size_t to, NFATransition trans);
void nfa_add_epsilon(NFA *nfa, size_t from, size_t to);
size_t* nfa_epsilon_closure(NFA *nfa, size_t *states, size_t count, size_t *result_count);
void nfa_print_transition_table(NFA *nfa);
char* nfa_to_dot(NFA *nfa);

#endif
