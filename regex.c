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

// 核心匹配函数：从指定位置开始匹配
static bool match_at_position(Regex *regex, const char *text, size_t pos, RegexMatch *match) {
    if (!regex || !text || !regex->nfa) return false;
    
    NFA *nfa = regex->nfa;
    size_t text_len = strlen(text);
    
    // 使用数组存储状态集合
    size_t *current_states = malloc(nfa->state_count * sizeof(size_t));
    size_t current_count = 0;
    
    // 初始状态：起始状态的 ε-closure
    size_t init_state = nfa->start_state;
    size_t init_closure_count;
    size_t *init_closure = nfa_epsilon_closure(nfa, &init_state, 1, &init_closure_count);
    
    for (size_t i = 0; i < init_closure_count; i++) {
        if (init_closure[i] < nfa->state_count) {
            current_states[current_count++] = init_closure[i];
        }
    }
    free(init_closure);
    
    size_t start_pos = pos;
    size_t end_pos = pos;
    bool accepted = false;
    
    // 检查空匹配（但不要立即返回，尝试匹配更多）
    for (size_t i = 0; i < current_count; i++) {
        if (current_states[i] == nfa->accept_state) {
            accepted = true;
            end_pos = pos;
            break;
        }
    }
    
    // 尝试匹配更多字符（贪心）
    size_t best_end = pos;
    bool best_accepted = accepted;
    size_t original_pos = pos;
    
    // 处理字符 - 尽可能多地匹配
    while (pos < text_len) {
        char c = text[pos];
        size_t *next_states = malloc(nfa->state_count * sizeof(size_t));
        size_t next_count = 0;
        
        // 对当前所有状态，查找匹配字符的转移
        for (size_t i = 0; i < current_count; i++) {
            size_t state = current_states[i];
            for (size_t j = 0; j < nfa->edge_count; j++) {
                if (nfa->edges[j].from == state) {
                    if (char_matches_transition(&nfa->edges[j].transition, c)) {
                        // 检查是否已经存在
                        bool already = false;
                        for (size_t k = 0; k < next_count; k++) {
                            if (next_states[k] == nfa->edges[j].to) {
                                already = true;
                                break;
                            }
                        }
                        if (!already) {
                            next_states[next_count++] = nfa->edges[j].to;
                        }
                    }
                }
            }
        }
        
        if (next_count == 0) {
            // 没有转移，停止
            free(next_states);
            break;
        }
        
        // 计算 ε-closure
        size_t next_closure_count;
        size_t *next_closure = nfa_epsilon_closure(nfa, next_states, next_count, &next_closure_count);
        free(next_states);
        
        // 更新当前状态
        current_count = 0;
        for (size_t i = 0; i < next_closure_count; i++) {
            if (next_closure[i] < nfa->state_count) {
                current_states[current_count++] = next_closure[i];
            }
        }
        free(next_closure);
        
        pos++;
        
        // 检查是否到达接受状态
        bool current_accepted = false;
        for (size_t i = 0; i < current_count; i++) {
            if (current_states[i] == nfa->accept_state) {
                current_accepted = true;
                break;
            }
        }
        
        if (current_accepted) {
            // 贪心：记录最长的匹配
            best_accepted = true;
            best_end = pos;
            accepted = true;
            end_pos = pos;
        }
        
        // 如果没有接受状态，但之前接受过，保留之前的最佳结果
        // 继续尝试匹配更多字符
    }
    
    // 如果文本结束，检查是否接受
    if (pos == text_len) {
        for (size_t i = 0; i < current_count; i++) {
            if (current_states[i] == nfa->accept_state) {
                accepted = true;
                if (pos > best_end) {
                    best_end = pos;
                }
                break;
            }
        }
    }
    
    free(current_states);
    
    // 使用最长的匹配
    if (best_accepted) {
        if (match) {
            match->start = start_pos;
            match->end = best_end;
            match->group_count = 0;
            match->groups = NULL;
        }
        return true;
    }
    
    // 尝试原始位置（空匹配）
    if (accepted && match) {
        match->start = start_pos;
        match->end = end_pos;
        match->group_count = 0;
        match->groups = NULL;
        return true;
    }
    
    return false;
}
bool regex_match(Regex *regex, const char *text, RegexMatch *match) {
    if (!regex || !text) return false;
    return match_at_position(regex, text, 0, match);
}

bool regex_search(Regex *regex, const char *text, RegexMatch *match) {
    if (!regex || !text) return false;
    
    size_t len = strlen(text);
    
    // 尝试从每个位置开始匹配
    for (size_t i = 0; i < len; i++) {
        if (match_at_position(regex, text, i, match)) {
            return true;
        }
    }
    
    // 尝试空字符串匹配
    RegexMatch empty_match;
    if (match_at_position(regex, "", 0, &empty_match)) {
        if (match) {
            match->start = len;
            match->end = len;
            match->group_count = 0;
            match->groups = NULL;
        }
        return true;
    }
    
    return false;
}

RegexMatches* regex_findall(Regex *regex, const char *text) {
    RegexMatches *matches = calloc(1, sizeof(RegexMatches));
    if (!regex || !text) return matches;
    
    size_t len = strlen(text);
    size_t pos = 0;
    RegexMatch match;
    
    while (pos < len) {
        if (match_at_position(regex, text, pos, &match)) {
            // 避免无限循环
            if (match.start == match.end) {
                // 空匹配，移动到下一个位置
                pos++;
                continue;
            }
            
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
