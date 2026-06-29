#include "../include/nfa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

NFA* nfa_new(void) {
    NFA *nfa = calloc(1, sizeof(NFA));
    return nfa;
}

void nfa_free(NFA *nfa) {
    if (nfa) {
        free(nfa->states);
        free(nfa->edges);
        free(nfa->capture_group_states);
        free(nfa);
    }
}

void nfa_add_state(NFA *nfa, bool is_accept) {
    if (nfa->state_count >= nfa->state_capacity) {
        nfa->state_capacity = nfa->state_capacity ? nfa->state_capacity * 2 : 8;
        nfa->states = realloc(nfa->states, nfa->state_capacity * sizeof(NFAState));
    }
    nfa->states[nfa->state_count].id = nfa->state_count;
    nfa->states[nfa->state_count].is_accept = is_accept;
    nfa->state_count++;
}

void nfa_add_edge(NFA *nfa, size_t from, size_t to, NFATransition trans) {
    if (nfa->edge_count >= nfa->edge_capacity) {
        nfa->edge_capacity = nfa->edge_capacity ? nfa->edge_capacity * 2 : 16;
        nfa->edges = realloc(nfa->edges, nfa->edge_capacity * sizeof(NFAEdge));
    }
    nfa->edges[nfa->edge_count].from = from;
    nfa->edges[nfa->edge_count].to = to;
    nfa->edges[nfa->edge_count].transition = trans;
    nfa->edge_count++;
}

void nfa_add_epsilon(NFA *nfa, size_t from, size_t to) {
    NFATransition trans = {.type = TRANS_EPSILON};
    nfa_add_edge(nfa, from, to, trans);
}

static size_t build_ast(NFA *nfa, AstNode *node) {
    if (!node) return 0;
    
    size_t start = nfa->state_count;
    nfa_add_state(nfa, false);
    
    switch (node->type) {
        case AST_CHAR: {
            size_t end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_CHAR, .ch = node->ch};
            nfa_add_edge(nfa, start, end, trans);
            return end;
        }
        case AST_ANY_CHAR: {
            size_t end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_ANY_CHAR};
            nfa_add_edge(nfa, start, end, trans);
            return end;
        }
        case AST_CONCAT: {
            size_t current = start;
            for (size_t i = 0; i < node->concat.child_count; i++) {
                size_t child_end = build_ast(nfa, node->concat.children[i]);
                nfa_add_epsilon(nfa, current, child_end);
                current = child_end;
            }
            return current;
        }
        case AST_ALTERNATION: {
            size_t end = nfa->state_count;
            nfa_add_state(nfa, false);
            
            for (size_t i = 0; i < node->alternation.child_count; i++) {
                size_t child_end = build_ast(nfa, node->alternation.children[i]);
                nfa_add_epsilon(nfa, start, child_end);
                nfa_add_epsilon(nfa, child_end, end);
            }
            return end;
        }
        case AST_REPEAT: {
            size_t end = nfa->state_count;
            nfa_add_state(nfa, false);
            
            if (node->repeat.min == 0 && node->repeat.max == 0) {
                // *: 0次或多次
                nfa_add_epsilon(nfa, start, end);
                size_t inner_end = build_ast(nfa, node->repeat.child);
                nfa_add_epsilon(nfa, start, inner_end);
                nfa_add_epsilon(nfa, inner_end, inner_end);
                nfa_add_epsilon(nfa, inner_end, end);
            } else if (node->repeat.min == 1 && node->repeat.max == 0) {
                // +: 1次或多次
                size_t inner_end = build_ast(nfa, node->repeat.child);
                nfa_add_epsilon(nfa, start, inner_end);
                nfa_add_epsilon(nfa, inner_end, inner_end);
                nfa_add_epsilon(nfa, inner_end, end);
            } else if (node->repeat.min == 0 && node->repeat.max == 1) {
                // ?: 0次或1次
                nfa_add_epsilon(nfa, start, end);
                size_t inner_end = build_ast(nfa, node->repeat.child);
                nfa_add_epsilon(nfa, start, inner_end);
                nfa_add_epsilon(nfa, inner_end, end);
            } else {
                // {m,n}: m到n次
                size_t current = start;
                size_t max = node->repeat.max > 0 ? node->repeat.max : node->repeat.min;
                for (size_t i = 0; i < max; i++) {
                    size_t inner_end = build_ast(nfa, node->repeat.child);
                    nfa_add_epsilon(nfa, current, inner_end);
                    current = inner_end;
                    if (i >= node->repeat.min) {
                        nfa_add_epsilon(nfa, current, end);
                    }
                }
                nfa_add_epsilon(nfa, current, end);
            }
            return end;
        }
        case AST_GROUP: {
            // 创建分组开始状态
            size_t group_start = nfa->state_count;
            nfa_add_state(nfa, false);
            
            // 记录分组开始
            nfa->capture_group_states = realloc(nfa->capture_group_states, 
                (nfa->capture_group_count + 2) * sizeof(size_t));
            nfa->capture_group_states[nfa->capture_group_count++] = group_start;
            
            // 构建内部表达式
            size_t inner_end = build_ast(nfa, node->group.child);
            
            // 创建分组结束状态
            size_t group_end = nfa->state_count;
            nfa_add_state(nfa, false);
            
            // 记录分组结束
            nfa->capture_group_states[nfa->capture_group_count++] = group_end;
            
            // 创建最终的结束状态
            size_t final_end = nfa->state_count;
            nfa_add_state(nfa, false);
            
            // 连接：start -> group_start -> inner -> group_end -> final_end
            nfa_add_epsilon(nfa, start, group_start);
            nfa_add_epsilon(nfa, inner_end, group_end);
            nfa_add_epsilon(nfa, group_end, final_end);
            
            return final_end;
        }
        case AST_CHAR_CLASS: {
            size_t end = nfa->state_count;
            nfa_add_state(nfa, false);
            
            NFATransition trans = {.type = TRANS_CHAR_CLASS};
            trans.char_class.negated = node->char_class.negated;
            trans.char_class.chars = node->char_class.chars;
            trans.char_class.chars_len = node->char_class.chars_len;
            trans.char_class.ranges = node->char_class.ranges;
            trans.char_class.ranges_len = node->char_class.ranges_len;
            
            nfa_add_edge(nfa, start, end, trans);
            return end;
        }
        default: {
            // 对于未处理的类型，直接返回结束状态
            size_t end = nfa->state_count;
            nfa_add_state(nfa, false);
            nfa_add_epsilon(nfa, start, end);
            return end;
        }
    }
}

NFA* nfa_from_ast(AstNode *ast) {
    NFA *nfa = nfa_new();
    
    // 重新初始化
    nfa->state_count = 0;
    nfa->edge_count = 0;
    nfa->capture_group_count = 0;
    free(nfa->states);
    free(nfa->edges);
    free(nfa->capture_group_states);
    nfa->states = NULL;
    nfa->edges = NULL;
    nfa->capture_group_states = NULL;
    nfa->state_capacity = 0;
    nfa->edge_capacity = 0;
    
    // 创建起始状态
    nfa->start_state = 0;
    nfa_add_state(nfa, false);
    
    // 构建 AST
    nfa->accept_state = build_ast(nfa, ast);
    
    // 确保接受状态被标记
    if (nfa->accept_state < nfa->state_count) {
        nfa->states[nfa->accept_state].is_accept = true;
    }
    
    return nfa;
}

size_t* nfa_epsilon_closure(NFA *nfa, size_t *states, size_t count, size_t *result_count) {
    if (count == 0) {
        *result_count = 0;
        return NULL;
    }
    
    bool *visited = calloc(nfa->state_count, sizeof(bool));
    size_t *stack = malloc(count * sizeof(size_t));
    size_t stack_top = 0;
    
    // 初始化栈和访问标记
    for (size_t i = 0; i < count; i++) {
        if (!visited[states[i]]) {
            visited[states[i]] = true;
            stack[stack_top++] = states[i];
        }
    }
    
    // 结果数组
    size_t *closure = malloc(nfa->state_count * sizeof(size_t));
    *result_count = 0;
    
    // 复制初始状态到结果
    for (size_t i = 0; i < count; i++) {
        if (!visited[states[i]]) {
            visited[states[i]] = true;
            closure[(*result_count)++] = states[i];
        }
    }
    
    // DFS 查找所有 ε-转移
    while (stack_top > 0) {
        size_t state = stack[--stack_top];
        
        for (size_t i = 0; i < nfa->edge_count; i++) {
            if (nfa->edges[i].from == state && nfa->edges[i].transition.type == TRANS_EPSILON) {
                size_t to = nfa->edges[i].to;
                if (!visited[to]) {
                    visited[to] = true;
                    stack[stack_top++] = to;
                    closure[(*result_count)++] = to;
                }
            }
        }
    }
    
    free(visited);
    free(stack);
    return closure;
}
void nfa_print_transition_table(NFA *nfa) {
    printf("=== NFA 状态转移表 ===\n");
    printf("起始状态: %zu, 接受状态: %zu\n", nfa->start_state, nfa->accept_state);
    printf("\n状态\t| 转移\n");
    printf("--------+--------\n");
    
    for (size_t i = 0; i < nfa->state_count; i++) {
        printf("%zu\t| ", i);
        bool first = true;
        for (size_t j = 0; j < nfa->edge_count; j++) {
            if (nfa->edges[j].from == i) {
                if (!first) printf(", ");
                first = false;
                
                char label[32];
                switch (nfa->edges[j].transition.type) {
                    case TRANS_EPSILON: snprintf(label, sizeof(label), "ε"); break;
                    case TRANS_CHAR: snprintf(label, sizeof(label), "'%c'", nfa->edges[j].transition.ch); break;
                    case TRANS_ANY_CHAR: snprintf(label, sizeof(label), "."); break;
                    case TRANS_DIGIT: snprintf(label, sizeof(label), "\\d"); break;
                    case TRANS_WORD: snprintf(label, sizeof(label), "\\w"); break;
                    case TRANS_WHITESPACE: snprintf(label, sizeof(label), "\\s"); break;
                    case TRANS_CHAR_CLASS: snprintf(label, sizeof(label), "[...]"); break;
                    default: snprintf(label, sizeof(label), "?"); break;
                }
                printf("%s->%zu", label, nfa->edges[j].to);
            }
        }
        if (first) printf("(无转移)");
        printf("\n");
    }
}

char* nfa_to_dot(NFA *nfa) {
    char *dot = malloc(1024 * 1024);
    if (!dot) return NULL;
    
    size_t pos = 0;
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "digraph NFA {\n");
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "  rankdir=LR;\n");
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "  start [shape=point];\n");
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "  start -> %zu;\n", nfa->start_state);
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "  %zu [shape=doublecircle];\n", nfa->accept_state);
    
    for (size_t i = 0; i < nfa->edge_count; i++) {
        char label[32];
        switch (nfa->edges[i].transition.type) {
            case TRANS_EPSILON: snprintf(label, sizeof(label), "ε"); break;
            case TRANS_CHAR: snprintf(label, sizeof(label), "%c", nfa->edges[i].transition.ch); break;
            case TRANS_ANY_CHAR: snprintf(label, sizeof(label), "."); break;
            case TRANS_DIGIT: snprintf(label, sizeof(label), "\\d"); break;
            case TRANS_WORD: snprintf(label, sizeof(label), "\\w"); break;
            case TRANS_WHITESPACE: snprintf(label, sizeof(label), "\\s"); break;
            default: snprintf(label, sizeof(label), "?"); break;
        }
        pos += snprintf(dot + pos, 1024 * 1024 - pos, "  %zu -> %zu [label=\"%s\"];\n", 
                       nfa->edges[i].from, nfa->edges[i].to, label);
    }
    
    pos += snprintf(dot + pos, 1024 * 1024 - pos, "}\n");
    return dot;
}
