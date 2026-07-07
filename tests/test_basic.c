#include "../include/myregex.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_basic_match() {
    const char *pattern = "a+b";
    const char *text = "aaab";
    
    printf("\n========================================\n");
    printf("测试模式: %s\n", pattern);
    printf("文本: %s\n", text);
    printf("========================================\n\n");
    
    // ===== 测试 NFA 模式 =====
    printf("=== NFA 模式 ===\n\n");
    
    Regex *regex_nfa = regex_compile(pattern, REGEX_MODE_NFA, NULL);
    assert(regex_nfa != NULL);
    regex_get_transition_table(regex_nfa);
    
    RegexMatch match;
    bool result = regex_match(regex_nfa, text, &match);
    printf("\n匹配结果: %s\n", result ? "成功" : "失败");
    if (result) {
        printf("匹配位置: [%zu, %zu)\n", match.start, match.end);
    }
    printf("\n");
    regex_free(regex_nfa);
    
    // ===== 测试 DFA 模式 =====
    printf("=== DFA 模式 ===\n\n");
    
    Regex *regex_dfa = regex_compile(pattern, REGEX_MODE_DFA, NULL);
    assert(regex_dfa != NULL);
    regex_get_transition_table(regex_dfa);
    
    result = regex_match(regex_dfa, text, &match);
    printf("\n匹配结果: %s\n", result ? "成功" : "失败");
    if (result) {
        printf("匹配位置: [%zu, %zu)\n", match.start, match.end);
    }
    printf("\n");
    regex_free(regex_dfa);
}

void test_alternation() {
    const char *pattern = "a|b";
    
    printf("\n========================================\n");
    printf("测试模式: %s\n", pattern);
    printf("========================================\n\n");
    
    printf("=== NFA 模式 ===\n\n");
    Regex *regex = regex_compile(pattern, REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    regex_get_transition_table(regex);
    regex_free(regex);
    
    printf("\n=== DFA 模式 ===\n\n");
    regex = regex_compile(pattern, REGEX_MODE_DFA, NULL);
    assert(regex != NULL);
    regex_get_transition_table(regex);
    regex_free(regex);
}

void test_repetition() {
    const char *pattern = "a*";
    const char *text = "aaa";
    
    printf("\n========================================\n");
    printf("测试模式: %s\n", pattern);
    printf("文本: %s\n", text);
    printf("========================================\n\n");
    
    printf("=== NFA 模式 ===\n\n");
    Regex *regex = regex_compile(pattern, REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    regex_get_transition_table(regex);
    
    RegexMatch match;
    bool result = regex_match(regex, text, &match);
    printf("\n匹配结果: %s\n", result ? "成功" : "失败");
    if (result) {
        printf("匹配位置: [%zu, %zu)\n", match.start, match.end);
    }
    printf("\n");
    regex_free(regex);
    
    printf("=== DFA 模式 ===\n\n");
    regex = regex_compile(pattern, REGEX_MODE_DFA, NULL);
    assert(regex != NULL);
    regex_get_transition_table(regex);
    
    result = regex_match(regex, text, &match);
    printf("\n匹配结果: %s\n", result ? "成功" : "失败");
    if (result) {
        printf("匹配位置: [%zu, %zu)\n", match.start, match.end);
    }
    printf("\n");
    regex_free(regex);
}

void test_complex_pattern() {
    const char *pattern = "(a|b)*c";
    const char *text = "aaabbbabc";
    
    printf("\n========================================\n");
    printf("测试模式: %s\n", pattern);
    printf("文本: %s\n", text);
    printf("========================================\n\n");
    
    printf("=== NFA 模式 ===\n\n");
    Regex *regex = regex_compile(pattern, REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    regex_get_transition_table(regex);
    
    RegexMatch match;
    bool result = regex_match(regex, text, &match);
    printf("\n匹配结果: %s\n", result ? "成功" : "失败");
    if (result) {
        printf("匹配位置: [%zu, %zu)\n", match.start, match.end);
    }
    printf("\n");
    regex_free(regex);
    
    printf("=== DFA 模式 ===\n\n");
    regex = regex_compile(pattern, REGEX_MODE_DFA, NULL);
    assert(regex != NULL);
    regex_get_transition_table(regex);
    regex_free(regex);
}

void test_search_and_findall() {
    const char *pattern = "\\d+";
    const char *text = "abc123def456ghi";
    
    printf("\n========================================\n");
    printf("测试模式: %s\n", pattern);
    printf("文本: %s\n", text);
    printf("========================================\n\n");
    
    printf("=== DFA 模式 (搜索/查找) ===\n\n");
    Regex *regex = regex_compile(pattern, REGEX_MODE_DFA, NULL);
    assert(regex != NULL);
    regex_get_transition_table(regex);
    
    RegexMatch match;
    bool result = regex_search(regex, text, &match);
    printf("\n搜索匹配结果: %s\n", result ? "成功" : "失败");
    if (result) {
        printf("匹配位置: [%zu, %zu)\n", match.start, match.end);
    }
    
    RegexMatches *matches = regex_findall(regex, text);
    printf("\n查找全部结果:\n");
    printf("找到 %zu 个匹配\n", matches->count);
    for (size_t i = 0; i < matches->count; i++) {
        printf("  Match %zu: [%zu, %zu)\n", i+1, 
               matches->matches[i].start, matches->matches[i].end);
    }
    
    regex_matches_free(matches);
    regex_free(regex);
    printf("\n");
}

int main() {
    printf("\n正则表达式引擎测试报告\n");
    printf("NFA状态转移表 + DFA状态转移表 + 匹配结果\n");
    printf("========================================\n");
    
    test_basic_match();
    test_alternation();
    test_repetition();
    test_complex_pattern();
    test_search_and_findall();
    
    printf("\n========================================\n");
    printf("所有测试通过\n");
    printf("\n");
    
    return 0;
}
