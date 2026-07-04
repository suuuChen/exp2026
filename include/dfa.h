#pragma once
#ifndef DFA_H
#define DFA_H

#include <stdbool.h>
#include <stddef.h>
#include "nfa.h"
#include "ast.h"
#include "myregex.h"

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
    
    // 新增：二维数组加速转移表
    int **transition_table;   // [state][char] = next_state, -1 表示无转移
    size_t alphabet_size;      // 字母表大小（通常为 128）
} DFA;

DFA* dfa_new(void);
void dfa_free(DFA *dfa);
DFA* dfa_from_nfa(NFA *nfa);
DFA* dfa_minimize(DFA *dfa);
void dfa_print_transition_table(DFA *dfa);
char* dfa_to_dot(DFA *dfa);
bool dfa_match_text(DFA *dfa, const char *text, size_t start_pos, RegexMatch *match);

// 新增：用于测试的辅助函数
size_t dfa_get_state_count(DFA *dfa);
void dfa_free_transition_table(DFA *dfa);

#endif
