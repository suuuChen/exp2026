#include "../include/regex.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_basic_match() {
    const char *pattern = "a+b";
    const char *text = "aaab";
    
    printf("Testing pattern: '%s' on text: '%s'\n", pattern, text);
    
    Regex *regex = regex_compile(pattern, REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    
    // 打印 NFA 转移表用于调试
    printf("\nNFA Transition Table:\n");
    regex_get_transition_table(regex);
    
    RegexMatch match;
    bool result = regex_match(regex, text, &match);
    printf("\nMatch result: %s\n", result ? "true" : "false");
    
    if (result) {
        printf("Match: start=%zu, end=%zu\n", match.start, match.end);
    }
    
    assert(result);
    assert(match.start == 0);
    assert(match.end == 4);
    
    printf("✓ Basic match test passed\n");
    regex_free(regex);
}

void test_alternation() {
    Regex *regex = regex_compile("a|b", REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    
    RegexMatch match;
    assert(regex_match(regex, "a", &match));
    assert(match.start == 0 && match.end == 1);
    assert(regex_match(regex, "b", &match));
    assert(match.start == 0 && match.end == 1);
    assert(!regex_match(regex, "c", &match));
    
    printf("✓ Alternation test passed\n");
    regex_free(regex);
}

void test_repetition() {
    Regex *regex = regex_compile("a*", REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    
    RegexMatch match;
    assert(regex_match(regex, "", &match));
    assert(regex_match(regex, "aaa", &match));
    assert(match.start == 0 && match.end == 3);
    
    printf("✓ Repetition test passed\n");
    regex_free(regex);
}

void test_complex_pattern() {
    Regex *regex = regex_compile("(a|b)*c", REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    
    RegexMatch match;
    assert(regex_match(regex, "aaabbbabc", &match));
    assert(match.start == 0 && match.end == 9);
    
    printf("✓ Complex pattern test passed\n");
    regex_free(regex);
}

void test_search_and_findall() {
    Regex *regex = regex_compile("\\d+", REGEX_MODE_NFA, NULL);
    assert(regex != NULL);
    
    const char *text = "abc123def456ghi";
    RegexMatch match;
    
    assert(regex_search(regex, text, &match));
    assert(match.start == 3 && match.end == 6);
    
    RegexMatches *matches = regex_findall(regex, text);
    assert(matches->count == 2);
    assert(matches->matches[0].start == 3 && matches->matches[0].end == 6);
    assert(matches->matches[1].start == 9 && matches->matches[1].end == 12);
    
    printf("✓ Search and findall test passed\n");
    regex_matches_free(matches);
    regex_free(regex);
}

int main() {
    printf("Running regex engine tests...\n");
    printf("================================\n\n");
    
    test_basic_match();
    test_alternation();
    test_repetition();
    test_complex_pattern();
    test_search_and_findall();
    
    printf("\n✓ All tests passed!\n");
    return 0;
}
