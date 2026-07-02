/*#include "../include/dfa.h"*/

#include "../include/dfa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DFA* dfa_new(void) {
    DFA* dfa = calloc(1, sizeof(DFA));
    return dfa;
}

void dfa_free(DFA* dfa) {
    if (dfa) {
        free(dfa->states);
        free(dfa->accept_states);
        free(dfa->transitions);
        free(dfa);
    }
}

static bool is_match_transition(NFATransition* trans, char c) {
    switch (trans->type) {
    case TRANS_EPSILON: return false;
    case TRANS_CHAR: return trans->ch == c;
    case TRANS_ANY_CHAR: return true;
    case TRANS_DIGIT: return c >= '0' && c <= '9';
    case TRANS_WORD: return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
    case TRANS_WHITESPACE: return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    case TRANS_CHAR_CLASS: {
        bool matched = false;
        for (size_t i = 0; i < trans->char_class.chars_len; i++) {
            if (trans->char_class.chars[i] == c) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            for (size_t i = 0; i < trans->char_class.ranges_len; i++) {
                if (c >= trans->char_class.ranges[i].start &&
                    c <= trans->char_class.ranges[i].end) {
                    matched = true;
                    break;
                }
            }
        }
        return trans->char_class.negated ? !matched : matched;
    }
    default: return false;
    }
}

// 修复：重命名函数避免混淆，并添加正确的参数
static bool is_state_in_set(size_t* states, size_t count, size_t state) {
    for (size_t i = 0; i < count; i++) {
        if (states[i] == state) return true;
    }
    return false;
}

static bool states_equal(size_t* a, size_t a_count, size_t* b, size_t b_count) {
    if (a_count != b_count) return false;
    for (size_t i = 0; i < a_count; i++) {
        if (!is_state_in_set(b, b_count, a[i])) return false;
    }
    return true;
}

DFA* dfa_from_nfa(NFA* nfa) {
    DFA* dfa = dfa_new();

    // 初始化起始状态
    size_t start_count = 1;
    size_t* start_set = malloc(sizeof(size_t));
    start_set[0] = nfa->start_state;
    size_t closure_count;
    size_t* start_closure = nfa_epsilon_closure(nfa, start_set, 1, &closure_count);
    free(start_set);

    // 映射NFA状态集合到DFA状态
    size_t** state_sets = NULL;
    size_t* set_counts = NULL;
    size_t set_count = 0;

    // 添加起始状态
    state_sets = realloc(state_sets, sizeof(size_t*));
    set_counts = realloc(set_counts, sizeof(size_t));
    state_sets[0] = start_closure;
    set_counts[0] = closure_count;
    set_count = 1;

    // BFS构建DFA
    size_t* queue = malloc(sizeof(size_t));
    size_t queue_size = 1;
    queue[0] = 0;

    while (queue_size > 0) {
        size_t dfa_state = queue[--queue_size];
        size_t* nfa_states = state_sets[dfa_state];
        size_t nfa_count = set_counts[dfa_state];

        // ========== 修复：直接使用全部ASCII 0~127作为输入符号 ==========
        char symbols[256];
        size_t symbol_count = 128;
        for (int c = 0; c < 128; c++) {
            symbols[c] = (char)c;
        }

        // 对每个字符计算转移
        for (size_t si = 0; si < symbol_count; si++) {
            char c = symbols[si];
            size_t* next_set = malloc(nfa->state_count * sizeof(size_t));
            size_t next_count = 0;

            for (size_t i = 0; i < nfa_count; i++) {
                size_t state = nfa_states[i];
                for (size_t j = 0; j < nfa->edge_count; j++) {
                    if (nfa->edges[j].from == state &&
                        is_match_transition(&nfa->edges[j].transition, c)) {
                        if (!is_state_in_set(next_set, next_count, nfa->edges[j].to)) {
                            next_set[next_count++] = nfa->edges[j].to;
                        }
                    }
                }
            }

            if (next_count > 0) {
                size_t closure_count2;
                size_t* closure = nfa_epsilon_closure(nfa, next_set, next_count, &closure_count2);
                free(next_set);
                next_set = closure;
                next_count = closure_count2;
            }

            if (next_count > 0) {
                // 查找是否已存在
                size_t target = set_count;
                bool found = false;
                for (size_t i = 0; i < set_count; i++) {
                    if (states_equal(state_sets[i], set_counts[i], next_set, next_count)) {
                        target = i;
                        found = true;
                        free(next_set);
                        break;
                    }
                }

                if (!found) {
                    state_sets = realloc(state_sets, (set_count + 1) * sizeof(size_t*));
                    set_counts = realloc(set_counts, (set_count + 1) * sizeof(size_t));
                    state_sets[set_count] = next_set;
                    set_counts[set_count] = next_count;
                    queue = realloc(queue, (queue_size + 1) * sizeof(size_t));
                    queue[queue_size++] = set_count;
                    target = set_count;
                    set_count++;
                }

                // 添加转移
                dfa->transitions = realloc(dfa->transitions,
                    (dfa->transition_count + 1) * sizeof(*dfa->transitions));
                dfa->transitions[dfa->transition_count].from = dfa_state;
                dfa->transitions[dfa->transition_count].symbol = c;
                dfa->transitions[dfa->transition_count].to = target;
                dfa->transition_count++;
            }
        }
    }

    free(queue);

    // 构建DFA状态
    dfa->state_count = set_count;
    dfa->states = malloc(set_count * sizeof(DFAState));
    dfa->accept_states = malloc(set_count * sizeof(size_t));
    dfa->accept_count = 0;
    dfa->start_state = 0;

    for (size_t i = 0; i < set_count; i++) {
        dfa->states[i].id = i;
        dfa->states[i].is_accept = false;

        // 检查是否为接受状态
        for (size_t j = 0; j < set_counts[i]; j++) {
            if (state_sets[i][j] == nfa->accept_state) {
                dfa->states[i].is_accept = true;
                dfa->accept_states[dfa->accept_count++] = i;
                break;
            }
        }
    }

    // 清理
    for (size_t i = 0; i < set_count; i++) {
        free(state_sets[i]);
    }
    free(state_sets);
    free(set_counts);

    return dfa;
}

DFA* dfa_minimize(DFA* dfa) {
    // 简化实现：直接返回原DFA
    // 完整的Hopcroft算法实现较复杂，这里省略
    // 实际项目中应该实现完整的最小化算法
    printf("Warning: DFA minimization is not fully implemented yet.\n");
    return dfa;
}

void dfa_print_transition_table(DFA* dfa) {
    printf("=== DFA 状态转移表 ===\n");
    printf("起始状态: %zu\n", dfa->start_state);
    printf("接受状态: ");
    for (size_t i = 0; i < dfa->accept_count; i++) {
        printf("%zu ", dfa->accept_states[i]);
    }
    printf("\n\n状态\t| 转移\n");
    printf("--------+--------\n");

    for (size_t i = 0; i < dfa->state_count; i++) {
        printf("%zu\t| ", i);
        bool first = true;
        for (size_t j = 0; j < dfa->transition_count; j++) {
            if (dfa->transitions[j].from == i) {
                if (!first) printf(", ");
                first = false;
                printf("'%c'->%zu", dfa->transitions[j].symbol, dfa->transitions[j].to);
            }
        }
        if (first) printf("(无转移)");
        printf("\n");
    }
}

char* dfa_to_dot(DFA* dfa) {
    char* dot = malloc(1024 * 1024);
    if (!dot) return NULL;

    size_t pos = 0;
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "digraph DFA {\n");
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "  rankdir=LR;\n");
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "  start [shape=point];\n");
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "  start -> %zu;\n", dfa->start_state);

    for (size_t i = 0; i < dfa->accept_count; i++) {
        pos += snprintf(dot + pos, 1024 * 1024 - pos, "  %zu [shape=doublecircle];\n", dfa->accept_states[i]);
    }

    for (size_t i = 0; i < dfa->transition_count; i++) {
        pos += snprintf(dot + pos, 1024 * 1024 - pos, "  %zu -> %zu [label=\"%c\"];\n",
            dfa->transitions[i].from, dfa->transitions[i].to, dfa->transitions[i].symbol);
    }

    pos += snprintf(dot + pos, 1024 * 1024 - pos, "}\n");
    return dot;
}

bool dfa_match_text(DFA *dfa, const char *text, size_t start_pos, RegexMatch *match)
{
    if (!dfa || !text)
        return false;

    size_t cur_state = dfa->start_state;
    size_t text_len = strlen(text);
    size_t best_match_end = start_pos;
    bool has_accept = false;

    // 初始状态如果是接收态，允许空匹配
    if (dfa->states[cur_state].is_accept)
    {
        has_accept = true;
        best_match_end = start_pos;
    }

    for (size_t idx = start_pos; idx < text_len; idx++)
    {
        char c = text[idx];
        size_t next_state = (size_t)-1;

        // 遍历所有转移边寻找当前字符对应的跳转
        for (size_t i = 0; i < dfa->transition_count; i++)
        {
            if (dfa->transitions[i].from == cur_state && dfa->transitions[i].symbol == c)
            {
                next_state = dfa->transitions[i].to;
                break;
            }
        }

        // 没有可用转移，匹配终止
        if (next_state == (size_t)-1)
            break;

        cur_state = next_state;

        // 贪心保存最长合法匹配位置
        if (dfa->states[cur_state].is_accept)
        {
            has_accept = true;
            best_match_end = idx + 1;
        }
    }

    if (has_accept && match != NULL)
    {
        match->start = start_pos;
        match->end = best_match_end;
        match->group_count = 0;
        match->groups = NULL;
    }

    return has_accept;
}
