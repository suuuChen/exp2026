#include "../include/nfa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 函数原型声明
static void build_expr(NFA *nfa, AstNode *node, size_t *start, size_t *end);
static void build_atom(NFA *nfa, AstNode *node, size_t *start, size_t *end);
static void build_repeat(NFA *nfa, AstNode *node, size_t *start, size_t *end);
static void build_concat(NFA *nfa, AstNode *node, size_t *start, size_t *end);
static void build_alternation(NFA *nfa, AstNode *node, size_t *start, size_t *end);

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

// 构建单个原子
static void build_atom(NFA *nfa, AstNode *node, size_t *start, size_t *end) {
    *start = nfa->state_count;
    nfa_add_state(nfa, false);
    
    switch (node->type) {
        case AST_CHAR: {
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_CHAR, .ch = node->ch};
            nfa_add_edge(nfa, *start, *end, trans);
            break;
        }
        case AST_ANY_CHAR: {
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_ANY_CHAR};
            nfa_add_edge(nfa, *start, *end, trans);
            break;
        }
        case AST_DIGIT: {
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_DIGIT};
            nfa_add_edge(nfa, *start, *end, trans);
            break;
        }
        case AST_WORD: {
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_WORD};
            nfa_add_edge(nfa, *start, *end, trans);
            break;
        }
        case AST_WHITESPACE: {
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_WHITESPACE};
            nfa_add_edge(nfa, *start, *end, trans);
            break;
        }
        case AST_START:
        case AST_END: {
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            nfa_add_epsilon(nfa, *start, *end);
            break;
        }
        case AST_CHAR_CLASS: {
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            NFATransition trans = {.type = TRANS_CHAR_CLASS};
            trans.char_class.negated = node->char_class.negated;
            trans.char_class.chars = node->char_class.chars;
            trans.char_class.chars_len = node->char_class.chars_len;
            trans.char_class.ranges = node->char_class.ranges;
            trans.char_class.ranges_len = node->char_class.ranges_len;
            nfa_add_edge(nfa, *start, *end, trans);
            break;
        }
        case AST_GROUP: {
            // 记录分组开始
            nfa->capture_group_states = realloc(nfa->capture_group_states, 
                (nfa->capture_group_count + 2) * sizeof(size_t));
            nfa->capture_group_states[nfa->capture_group_count++] = *start;
            
            // 构建内部表达式
            build_expr(nfa, node->group.child, start, end);
            
            // 记录分组结束
            nfa->capture_group_states[nfa->capture_group_count++] = *end;
            break;
        }
        default: {
            *end = *start;
            break;
        }
    }
}

// 构建重复（量词）- 修复版本
static void build_repeat(NFA *nfa, AstNode *node, size_t *start, size_t *end) {
    size_t inner_start, inner_end;
    build_atom(nfa, node->repeat.child, &inner_start, &inner_end);

    *start = nfa->state_count;
    nfa_add_state(nfa, false);
    *end = nfa->state_count;
    nfa_add_state(nfa, false);

    size_t min = node->repeat.min;
    size_t max = node->repeat.max;

    // Kleene Star: A*  0次或多次
    if (min == 0 && max == SIZE_MAX) {
        nfa_add_epsilon(nfa, *start, *end);
        nfa_add_epsilon(nfa, *start, inner_start);
        nfa_add_epsilon(nfa, inner_end, inner_start);
        nfa_add_epsilon(nfa, inner_end, *end);
    }
    // A+ 1次或多次
    else if (min == 1 && max == SIZE_MAX) {
        nfa_add_epsilon(nfa, *start, inner_start);
        nfa_add_epsilon(nfa, inner_end, inner_start);
        nfa_add_epsilon(nfa, inner_end, *end);
    }
    // A? 0次或1次
    else if (min == 0 && max == 1) {
        nfa_add_epsilon(nfa, *start, *end);
        nfa_add_epsilon(nfa, *start, inner_start);
        nfa_add_epsilon(nfa, inner_end, *end);
    }
    // {m,n} 有限次数
    else {
        size_t cur_state = *start;
        // 必须匹配 min 次
        for (size_t i = 0; i < min; i++) {
            size_t s, e;
            build_atom(nfa, node->repeat.child, &s, &e);
            nfa_add_epsilon(nfa, cur_state, s);
            cur_state = e;
        }
        // 可选匹配 min ~ max 次
        if (max != SIZE_MAX) {
            for (size_t i = min; i < max; i++) {
                size_t s, e;
                build_atom(nfa, node->repeat.child, &s, &e);
                nfa_add_epsilon(nfa, cur_state, s);
                nfa_add_epsilon(nfa, cur_state, *end);
                cur_state = e;
            }
        }
        nfa_add_epsilon(nfa, cur_state, *end);
    }
}

// 构建连接
static void build_concat(NFA *nfa, AstNode *node, size_t *start, size_t *end) {
    size_t current_start, current_end;
    bool first = true;
    
    for (size_t i = 0; i < node->concat.child_count; i++) {
        size_t s, e;
        build_expr(nfa, node->concat.children[i], &s, &e);
        
        if (first) {
            current_start = s;
            current_end = e;
            first = false;
        } else {
            nfa_add_epsilon(nfa, current_end, s);
            current_end = e;
        }
    }
    
    if (first) {
        // 没有子节点
        *start = nfa->state_count;
        nfa_add_state(nfa, false);
        *end = nfa->state_count;
        nfa_add_state(nfa, false);
        nfa_add_epsilon(nfa, *start, *end);
    } else {
        *start = current_start;
        *end = current_end;
    }
}

// 构建选择
static void build_alternation(NFA *nfa, AstNode *node, size_t *start, size_t *end) {
    *start = nfa->state_count;
    nfa_add_state(nfa, false);
    *end = nfa->state_count;
    nfa_add_state(nfa, false);
    
    for (size_t i = 0; i < node->alternation.child_count; i++) {
        size_t s, e;
        build_expr(nfa, node->alternation.children[i], &s, &e);
        nfa_add_epsilon(nfa, *start, s);
        nfa_add_epsilon(nfa, e, *end);
    }
}

// 主要的表达式构建函数
static void build_expr(NFA *nfa, AstNode *node, size_t *start, size_t *end) {
    if (!node) {
        *start = nfa->state_count;
        nfa_add_state(nfa, false);
        *end = nfa->state_count;
        nfa_add_state(nfa, false);
        nfa_add_epsilon(nfa, *start, *end);
        return;
    }
    
    switch (node->type) {
        case AST_CHAR:
        case AST_ANY_CHAR:
        case AST_CHAR_CLASS:
        case AST_GROUP:
            build_atom(nfa, node, start, end);
            break;
            
        case AST_REPEAT:
            build_repeat(nfa, node, start, end);
            break;
            
        case AST_CONCAT:
            build_concat(nfa, node, start, end);
            break;
            
        case AST_ALTERNATION:
            build_alternation(nfa, node, start, end);
            break;
            
        default:
            *start = nfa->state_count;
            nfa_add_state(nfa, false);
            *end = nfa->state_count;
            nfa_add_state(nfa, false);
            nfa_add_epsilon(nfa, *start, *end);
            break;
    }
}

NFA* nfa_from_ast(AstNode *ast) {
    NFA *nfa = nfa_new();
    
    // 构建表达式
    size_t start, end;
    build_expr(nfa, ast, &start, &end);
    
    // 设置起始和接受状态
    nfa->start_state = start;
    
    // 添加接受状态
    size_t accept = nfa->state_count;
    nfa_add_state(nfa, true);
    nfa_add_epsilon(nfa, end, accept);
    nfa->accept_state = accept;
    
    return nfa;
}

size_t* nfa_epsilon_closure(NFA *nfa, size_t *states, size_t count, size_t *result_count) {
    if (count == 0 || !states) {
        *result_count = 0;
        return NULL;
    }

    bool *visited = calloc(nfa->state_count, sizeof(bool));
    size_t *stack = malloc(nfa->state_count * sizeof(size_t));
    size_t stack_top = 0;
    size_t *closure = malloc(nfa->state_count * sizeof(size_t));
    *result_count = 0;

    // 初始化：入栈 + 加入结果集
    for (size_t i = 0; i < count; i++) {
        size_t s = states[i];
        if (s < nfa->state_count && !visited[s]) {
            visited[s] = true;
            stack[stack_top++] = s;
            closure[(*result_count)++] = s;
        }
    }

    while (stack_top > 0) {
        size_t state = stack[--stack_top];
        for (size_t i = 0; i < nfa->edge_count; i++) {
            NFAEdge e = nfa->edges[i];
            if (e.from == state && e.transition.type == TRANS_EPSILON) {
                size_t to = e.to;
                if (to < nfa->state_count && !visited[to]) {
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
