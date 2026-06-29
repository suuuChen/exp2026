#include "../include/regex.h"
#include "../include/nfa.h"
#include "../include/dfa.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Regex {
    char *pattern;
    RegexMode mode;
    NFA *nfa;
    DFA *dfa;
    bool is_compiled;
};

// 前向声明
AstNode* regex_parse(const char *pattern, char **error);

Regex* regex_compile(const char *pattern, RegexMode mode, const char **error) {
    Regex *regex = calloc(1, sizeof(Regex));
    regex->pattern = strdup(pattern);
    regex->mode = mode;
    
    char *parse_error = NULL;
    AstNode *ast = regex_parse(pattern, &parse_error);
    if (!ast) {
        if (error) *error = parse_error;
        free(regex->pattern);
        free(regex);
        return NULL;
    }
    
    regex->nfa = nfa_from_ast(ast);
    ast_free(ast);
    
    if (mode == REGEX_MODE_DFA && regex->nfa) {
        regex->dfa = dfa_from_nfa(regex->nfa);
        if (regex->dfa) {
            regex->dfa = dfa_minimize(regex->dfa);
        }
    }
    
    regex->is_compiled = true;
    return regex;
}

void regex_free(Regex *regex) {
    if (regex) {
        free(regex->pattern);
        if (regex->nfa) nfa_free(regex->nfa);
        if (regex->dfa) dfa_free(regex->dfa);
        free(regex);
    }
}

// 检查字符是否匹配转移
static bool char_matches_transition(NFATransition *trans, char c) {
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

// 获取从某个状态出发的所有可能转移（包括ε-closure）
static void get_transitions(NFA *nfa, size_t state, char c, size_t **next_states, size_t *count) {
    for (size_t i = 0; i < nfa->edge_count; i++) {
        if (nfa->edges[i].from == state) {
            if (char_matches_transition(&nfa->edges[i].transition, c)) {
                *next_states = realloc(*next_states, (*count + 1) * sizeof(size_t));
                (*next_states)[*count] = nfa->edges[i].to;
                (*count)++;
            }
        }
    }
}

// 在 regex.c 中替换 regex_match 函数

// 辅助函数：检查状态是否在集合中
static bool state_in_set(size_t *states, size_t count, size_t state) {
    for (size_t i = 0; i < count; i++) {
        if (states[i] == state) return true;
    }
    return false;
}

// 辅助函数：合并两个状态集合
static size_t* merge_states(size_t *a, size_t a_count, size_t *b, size_t b_count, size_t *result_count) {
    size_t *result = malloc((a_count + b_count) * sizeof(size_t));
    *result_count = 0;
    
    // 复制 a
    for (size_t i = 0; i < a_count; i++) {
        if (!state_in_set(result, *result_count, a[i])) {
            result[(*result_count)++] = a[i];
        }
    }
    
    // 复制 b
    for (size_t i = 0; i < b_count; i++) {
        if (!state_in_set(result, *result_count, b[i])) {
            result[(*result_count)++] = b[i];
        }
    }
    
    return result;
}

bool regex_match(Regex *regex, const char *text, RegexMatch *match) {
    if (!regex || !text || !regex->nfa) return false;
    
    NFA *nfa = regex->nfa;
    
    // 初始状态集合：起始状态的 ε-closure
    size_t start_state = nfa->start_state;
    size_t *current_states = malloc(sizeof(size_t));
    current_states[0] = start_state;
    size_t current_count = 1;
    
    size_t closure_count;
    size_t *closure = nfa_epsilon_closure(nfa, current_states, current_count, &closure_count);
    free(current_states);
    current_states = closure;
    current_count = closure_count;
    
    size_t pos = 0;
    size_t text_len = strlen(text);
    
    // 如果文本为空，检查是否接受
    if (text_len == 0) {
        bool accepted = false;
        for (size_t i = 0; i < current_count; i++) {
            if (current_states[i] == nfa->accept_state) {
                accepted = true;
                break;
            }
        }
        free(current_states);
        if (accepted && match) {
            match->start = 0;
            match->end = 0;
            match->group_count = 0;
            match->groups = NULL;
        }
        return accepted;
    }
    
    // 处理每个字符
    while (pos < text_len) {
        char c = text[pos];
        size_t *next_states = NULL;
        size_t next_count = 0;
        
        // 对当前所有状态，计算在字符 c 下的转移
        for (size_t i = 0; i < current_count; i++) {
            size_t state = current_states[i];
            
            // 查找所有从 state 出发的转移
            for (size_t j = 0; j < nfa->edge_count; j++) {
                if (nfa->edges[j].from == state) {
                    if (char_matches_transition(&nfa->edges[j].transition, c)) {
                        // 添加目标状态
                        if (!state_in_set(next_states, next_count, nfa->edges[j].to)) {
                            next_states = realloc(next_states, (next_count + 1) * sizeof(size_t));
                            next_states[next_count++] = nfa->edges[j].to;
                        }
                    }
                }
            }
        }
        
        if (next_count == 0) {
            // 没有转移，匹配失败
            free(current_states);
            return false;
        }
        
        // 计算 ε-closure
        size_t next_closure_count;
        size_t *next_closure = nfa_epsilon_closure(nfa, next_states, next_count, &next_closure_count);
        free(next_states);
        
        free(current_states);
        current_states = next_closure;
        current_count = next_closure_count;
        pos++;
    }
    
    // 检查是否到达接受状态
    bool accepted = false;
    for (size_t i = 0; i < current_count; i++) {
        if (current_states[i] == nfa->accept_state) {
            accepted = true;
            break;
        }
    }
    
    free(current_states);
    
    if (accepted && match) {
        match->start = 0;
        match->end = pos;
        match->group_count = 0;
        match->groups = NULL;
    }
    
    return accepted;
}

bool regex_search(Regex *regex, const char *text, RegexMatch *match) {
    if (!regex || !text) return false;
    
    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        RegexMatch temp_match;
        if (regex_match(regex, text + i, &temp_match)) {
            if (match) {
                match->start = i + temp_match.start;
                match->end = i + temp_match.end;
                match->group_count = temp_match.group_count;
                match->groups = temp_match.groups;
            }
            return true;
        }
    }
    return false;
}

RegexMatches* regex_findall(Regex *regex, const char *text) {
    RegexMatches *matches = calloc(1, sizeof(RegexMatches));
    if (!regex || !text) return matches;
    
    size_t pos = 0;
    size_t len = strlen(text);
    RegexMatch match;
    
    while (pos < len) {
        if (regex_search(regex, text + pos, &match)) {
            match.start += pos;
            match.end += pos;
            matches->matches = realloc(matches->matches, 
                (matches->count + 1) * sizeof(RegexMatch));
            matches->matches[matches->count] = match;
            matches->count++;
            pos = match.end;
        } else {
            pos++;
        }
    }
    
    return matches;
}

void regex_match_free(RegexMatch *match) {
    if (match) {
        free(match->groups);
        match->groups = NULL;
        match->group_count = 0;
    }
}

void regex_matches_free(RegexMatches *matches) {
    if (matches) {
        if (matches->matches) {
            for (size_t i = 0; i < matches->count; i++) {
                free(matches->matches[i].groups);
            }
            free(matches->matches);
        }
        free(matches);
    }
}

char* regex_get_transition_table(Regex *regex) {
    if (!regex) return NULL;
    
    if (regex->mode == REGEX_MODE_NFA && regex->nfa) {
        nfa_print_transition_table(regex->nfa);
        return strdup("NFA transition table printed above");
    } else if (regex->dfa) {
        dfa_print_transition_table(regex->dfa);
        return strdup("DFA transition table printed above");
    }
    return strdup("No transition table available");
}

char* regex_to_dot(Regex *regex) {
    if (!regex) return NULL;
    
    if (regex->mode == REGEX_MODE_NFA && regex->nfa) {
        return nfa_to_dot(regex->nfa);
    } else if (regex->dfa) {
        return dfa_to_dot(regex->dfa);
    }
    return NULL;
}
