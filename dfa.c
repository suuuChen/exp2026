#include "../include/dfa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DFA* dfa_new(void) {
    DFA* dfa = calloc(1, sizeof(DFA));
    dfa->alphabet_size = 128;
    return dfa;
}

void dfa_free(DFA* dfa) {
    if (dfa) {
        free(dfa->states);
        free(dfa->accept_states);
        free(dfa->transitions);
        if (dfa->transition_table) {
            for (size_t i = 0; i < dfa->state_count; i++) {
                free(dfa->transition_table[i]);
            }
            free(dfa->transition_table);
        }
        free(dfa);
    }
}

static bool is_match_transition(NFATransition* trans, char c) {
    switch (trans->type) {
        case TRANS_EPSILON:
            return false;
        case TRANS_CHAR:
            return trans->ch == c;
        case TRANS_ANY_CHAR:
            return true;
        case TRANS_DIGIT:
            return c >= '0' && c <= '9';
        case TRANS_WORD:
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        case TRANS_WHITESPACE:
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
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
        default:
            return false;
    }
}

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

static void build_transition_table(DFA *dfa) {
    if (dfa->state_count == 0) return;
    
    dfa->alphabet_size = 128;
    dfa->transition_table = malloc(dfa->state_count * sizeof(int*));
    
    for (size_t i = 0; i < dfa->state_count; i++) {
        dfa->transition_table[i] = malloc(dfa->alphabet_size * sizeof(int));
        for (int c = 0; c < (int)dfa->alphabet_size; c++) {
            dfa->transition_table[i][c] = -1;
        }
    }
    
    for (size_t i = 0; i < dfa->transition_count; i++) {
        unsigned char symbol = (unsigned char)dfa->transitions[i].symbol;
        if (symbol < dfa->alphabet_size) {
            dfa->transition_table[dfa->transitions[i].from][symbol] = (int)dfa->transitions[i].to;
        }
    }
}

DFA* dfa_minimize(DFA *dfa) {
    if (!dfa || dfa->state_count <= 1) return dfa;
    
    size_t n = dfa->state_count;
    
    bool *is_accept = calloc(n, sizeof(bool));
    for (size_t i = 0; i < dfa->accept_count; i++) {
        is_accept[dfa->accept_states[i]] = true;
    }
    
    size_t *partition = malloc(n * sizeof(size_t));
    size_t partition_count = 0;
    
    for (size_t i = 0; i < n; i++) {
        if (is_accept[i]) {
            partition[i] = 0;
        } else {
            partition[i] = 1;
        }
    }
    partition_count = 2;
    
    bool changed;
    do {
        changed = false;
        
        size_t *new_partition = malloc(n * sizeof(size_t));
        memcpy(new_partition, partition, n * sizeof(size_t));
        size_t new_count = partition_count;
        
        for (size_t p = 0; p < partition_count; p++) {
            size_t *states_in_p = malloc(n * sizeof(size_t));
            size_t count_p = 0;
            for (size_t i = 0; i < n; i++) {
                if (partition[i] == p) {
                    states_in_p[count_p++] = i;
                }
            }
            
            if (count_p <= 1) {
                free(states_in_p);
                continue;
            }
            
            bool need_split = false;
            for (int c = 0; c < (int)dfa->alphabet_size && !need_split; c++) {
                int *target_partitions = malloc(count_p * sizeof(int));
                for (size_t i = 0; i < count_p; i++) {
                    size_t state = states_in_p[i];
                    int next = dfa->transition_table[state][c];
                    if (next >= 0 && (size_t)next < n) {
                        target_partitions[i] = partition[next];
                    } else {
                        target_partitions[i] = -1;
                    }
                }
                
                int first = target_partitions[0];
                for (size_t i = 1; i < count_p; i++) {
                    if (target_partitions[i] != first) {
                        need_split = true;
                        break;
                    }
                }
                free(target_partitions);
                
                if (need_split) {
                    for (size_t i = 1; i < count_p; i++) {
                        size_t state = states_in_p[i];
                        int next = dfa->transition_table[state][c];
                        int target_p = (next >= 0 && (size_t)next < n) ? partition[next] : -1;
                        if (target_p != partition[states_in_p[0]]) {
                            new_partition[state] = new_count;
                            changed = true;
                        }
                    }
                    if (changed) {
                        new_count++;
                    }
                }
            }
            free(states_in_p);
        }
        
        if (changed) {
            memcpy(partition, new_partition, n * sizeof(size_t));
            partition_count = new_count;
        }
        free(new_partition);
        
    } while (changed);
    
    DFA *min_dfa = dfa_new();
    min_dfa->alphabet_size = dfa->alphabet_size;
    
    size_t *state_count_in_partition = calloc(partition_count, sizeof(size_t));
    for (size_t i = 0; i < n; i++) {
        state_count_in_partition[partition[i]]++;
    }
    
    min_dfa->state_count = partition_count;
    min_dfa->states = malloc(partition_count * sizeof(DFAState));
    for (size_t i = 0; i < partition_count; i++) {
        min_dfa->states[i].id = i;
        min_dfa->states[i].is_accept = false;
    }
    
    min_dfa->accept_states = malloc(partition_count * sizeof(size_t));
    min_dfa->accept_count = 0;
    for (size_t i = 0; i < n; i++) {
        if (is_accept[i]) {
            size_t p = partition[i];
            if (!min_dfa->states[p].is_accept) {
                min_dfa->states[p].is_accept = true;
                min_dfa->accept_states[min_dfa->accept_count++] = p;
            }
        }
    }
    
    min_dfa->start_state = partition[dfa->start_state];
    
    for (size_t i = 0; i < n; i++) {
        size_t from_p = partition[i];
        for (int c = 0; c < (int)dfa->alphabet_size; c++) {
            int next = dfa->transition_table[i][c];
            if (next >= 0 && (size_t)next < n) {
                size_t to_p = partition[next];
                bool exists = false;
                for (size_t j = 0; j < min_dfa->transition_count; j++) {
                    if (min_dfa->transitions[j].from == from_p &&
                        min_dfa->transitions[j].symbol == (char)c &&
                        min_dfa->transitions[j].to == to_p) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    min_dfa->transitions = realloc(min_dfa->transitions,
                        (min_dfa->transition_count + 1) * sizeof(*min_dfa->transitions));
                    min_dfa->transitions[min_dfa->transition_count].from = from_p;
                    min_dfa->transitions[min_dfa->transition_count].symbol = (char)c;
                    min_dfa->transitions[min_dfa->transition_count].to = to_p;
                    min_dfa->transition_count++;
                }
            }
        }
    }
    
    build_transition_table(min_dfa);
    
    free(state_count_in_partition);
    free(partition);
    free(is_accept);
    
    printf("Hopcroft minimization: %zu states → %zu states (reduced %.1f%%)\n",
           n, min_dfa->state_count, 
           (1.0 - (double)min_dfa->state_count / n) * 100.0);
    
    return min_dfa;
}

DFA* dfa_from_nfa(NFA* nfa) {
    DFA* dfa = dfa_new();

    size_t start_count = 1;
    size_t* start_set = malloc(sizeof(size_t));
    start_set[0] = nfa->start_state;
    size_t closure_count;
    size_t* start_closure = nfa_epsilon_closure(nfa, start_set, 1, &closure_count);
    free(start_set);

    size_t** state_sets = NULL;
    size_t* set_counts = NULL;
    size_t set_count = 0;

    state_sets = realloc(state_sets, sizeof(size_t*));
    set_counts = realloc(set_counts, sizeof(size_t));
    state_sets[0] = start_closure;
    set_counts[0] = closure_count;
    set_count = 1;

    size_t* queue = malloc(sizeof(size_t));
    size_t queue_size = 1;
    queue[0] = 0;

    while (queue_size > 0) {
        size_t dfa_state = queue[--queue_size];
        size_t* nfa_states = state_sets[dfa_state];
        size_t nfa_count = set_counts[dfa_state];

        for (int c = 0; c < 128; c++) {
            char symbol = (char)c;
            size_t* next_set = malloc(nfa->state_count * sizeof(size_t));
            size_t next_count = 0;

            for (size_t i = 0; i < nfa_count; i++) {
                size_t state = nfa_states[i];
                for (size_t j = 0; j < nfa->edge_count; j++) {
                    if (nfa->edges[j].from == state &&
                        is_match_transition(&nfa->edges[j].transition, symbol)) {
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

                dfa->transitions = realloc(dfa->transitions,
                    (dfa->transition_count + 1) * sizeof(*dfa->transitions));
                dfa->transitions[dfa->transition_count].from = dfa_state;
                dfa->transitions[dfa->transition_count].symbol = symbol;
                dfa->transitions[dfa->transition_count].to = target;
                dfa->transition_count++;
            }
        }
    }

    free(queue);

    dfa->state_count = set_count;
    dfa->states = malloc(set_count * sizeof(DFAState));
    dfa->accept_states = malloc(set_count * sizeof(size_t));
    dfa->accept_count = 0;
    dfa->start_state = 0;

    for (size_t i = 0; i < set_count; i++) {
        dfa->states[i].id = i;
        dfa->states[i].is_accept = false;

        for (size_t j = 0; j < set_counts[i]; j++) {
            if (state_sets[i][j] == nfa->accept_state) {
                dfa->states[i].is_accept = true;
                dfa->accept_states[dfa->accept_count++] = i;
                break;
            }
        }
    }

    for (size_t i = 0; i < set_count; i++) {
        free(state_sets[i]);
    }
    free(state_sets);
    free(set_counts);

    build_transition_table(dfa);

    return dfa;
}

bool dfa_match_text(DFA *dfa, const char *text, size_t start_pos, RegexMatch *match) {
    if (!dfa || !text) return false;

    if (!dfa->transition_table) {
        return false;
    }

    size_t cur_state = dfa->start_state;
    size_t text_len = strlen(text);
    size_t best_match_end = start_pos;
    bool has_accept = dfa->states[cur_state].is_accept;

    for (size_t idx = start_pos; idx < text_len; idx++) {
        unsigned char c = (unsigned char)text[idx];
        if (c >= dfa->alphabet_size) {
            continue;
        }
        
        int next_state = dfa->transition_table[cur_state][c];
        if (next_state == -1) {
            break;
        }
        
        cur_state = (size_t)next_state;
        if (dfa->states[cur_state].is_accept) {
            has_accept = true;
            best_match_end = idx + 1;
        }
    }

    if (has_accept && match) {
        match->start = start_pos;
        match->end = best_match_end;
        match->group_count = 0;
        match->groups = NULL;
    }
    return has_accept;
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

size_t dfa_get_state_count(DFA *dfa) {
    return dfa ? dfa->state_count : 0;
}

void dfa_free_transition_table(DFA *dfa) {
    if (dfa && dfa->transition_table) {
        for (size_t i = 0; i < dfa->state_count; i++) {
            free(dfa->transition_table[i]);
        }
        free(dfa->transition_table);
        dfa->transition_table = NULL;
    }
}
