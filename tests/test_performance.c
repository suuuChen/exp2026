#include <regex.h>  // POSIX系统正则库
#include "../include/myregex.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

// 时间测量辅助函数
double get_time_ms(clock_t start, clock_t end) {
    return ((double)(end - start) * 1000.0) / CLOCKS_PER_SEC;
}

void performance_test(const char* pattern, const char* text, int iterations) {
    printf("Pattern: %s\n", pattern);
    printf("Text length: %zu\n", strlen(text));
    printf("Iterations: %d\n", iterations);
    printf("\n");

    // 测试我们的引擎 (DFA模式)
    clock_t start = clock();
    Regex* our_regex = regex_compile(pattern, REGEX_MODE_DFA, NULL);
    if (!our_regex) {
        printf("  Our engine: compilation failed\n");
        return;
    }

    double compile_time = get_time_ms(start, clock());

    start = clock();
    int our_matches = 0;
    for (int i = 0; i < iterations; i++) {
        RegexMatch match;
        if (regex_match(our_regex, text, &match)) {
            our_matches++;
        }
    }
    double our_time = get_time_ms(start, clock());
    regex_free(our_regex);

    printf("Compilation time:\n");
    printf("  Our engine: %.2f ms\n", compile_time);

    printf("\nMatching time (%d iterations):\n", iterations);
    printf("  Our engine: %.2f ms (%d matches)\n", our_time, our_matches);

    // POSIX regex 对比
    printf("\n--- POSIX Regex Comparison ---\n");

    start = clock();
    regex_t posix_regex;
    int posix_compile_success = regcomp(&posix_regex, pattern, REG_EXTENDED);
    double posix_compile_time = get_time_ms(start, clock());

    if (posix_compile_success != 0) {
        printf("  POSIX: compilation failed\n");
    }
    else {
        start = clock();
        int posix_matches = 0;
        for (int i = 0; i < iterations; i++) {
            regmatch_t posix_match[1];
            if (regexec(&posix_regex, text, 1, posix_match, 0) == 0) {
                posix_matches++;
            }
        }
        double posix_time = get_time_ms(start, clock());
        regfree(&posix_regex);

        printf("  Compilation: %.2f ms\n", posix_compile_time);
        printf("  Matching: %.2f ms (%d matches)\n", posix_time, posix_matches);

        // ============ 核心修改：计算速度占比 ============
        // 速度占比 = POSIX时间 / 我们的时间 × 100%
        // 表示：我们的速度是 POSIX 的百分之多少
        double speed_ratio = (posix_time / our_time) * 100.0;
        
        // 耗时比 = 我们的时间 / POSIX时间 × 100%
        double time_ratio = (our_time / posix_time) * 100.0;
        
        printf("\n--- 性能分析 ---\n");
        printf("  耗时比 (Our/POSIX): %.2f%%\n", time_ratio);
        printf("  速度占比 (POSIX/Our): %.2f%%\n", speed_ratio);
        
        // 判断是否达标（速度占比 ≥ 80%）
        if (speed_ratio >= 80.0) {
            printf("  ✅ 达标：速度达到 POSIX 的 %.2f%% (≥ 80%%)\n", speed_ratio);
        } else {
            printf("  ❌ 不达标：速度仅为 POSIX 的 %.2f%% (< 80%%)\n", speed_ratio);
        }
        
        // 额外显示倍数关系
        if (speed_ratio > 100.0) {
            printf("  🏆 比 POSIX 快 %.2f 倍\n", speed_ratio / 100.0);
        } else if (speed_ratio < 100.0) {
            printf("  ⚠️ 比 POSIX 慢 %.2f 倍\n", 100.0 / speed_ratio);
        } else {
            printf("  ✅ 与 POSIX 速度相同\n");
        }
    }

    printf("========================================\n\n");
}

void test_catastrophic_backtracking() {
    printf("=== Catastrophic Backtracking Tests ===\n\n");

    const char* patterns[] = {
        "a*a*a*a*a*a*aaaaaaaaaa",
        "a*b*c*d*e*f*g*h*i*j*k*",
        "(a|b|c|d|e|f|g|h)*"
    };

    const char* text = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    int iterations = 100;

    for (int i = 0; i < 3; i++) {
        performance_test(patterns[i], text, iterations);
    }
}

void test_simple_patterns() {
    printf("=== Simple Pattern Tests ===\n\n");

    struct {
        const char* pattern;
        const char* text;
    } tests[] = {
        {"a+b", "aaaaaaaaaaaaaaaaaaaaab"},
        {"[0-9]+", "1234567890"},
        {"(a|b|c)+", "abcabcabcabcabc"},
        {"a{2,5}", "aaaaa"}
    };

    int iterations = 1000;

    for (int i = 0; i < 4; i++) {
        performance_test(tests[i].pattern, tests[i].text, iterations);
    }
}

void test_search_operations() {
    printf("=== Search Operations Tests ===\n\n");

    const char* pattern = "\\d+";
    const char* text = "abc123def456ghi789jkl";
    int iterations = 1000;

    printf("Pattern: %s\n", pattern);
    printf("Text: %s\n", text);
    printf("Iterations: %d\n", iterations);
    printf("\n");

    Regex* regex = regex_compile(pattern, REGEX_MODE_DFA, NULL);
    if (!regex) {
        printf("  Compilation failed\n");
        return;
    }

    clock_t start = clock();
    int found_count = 0;
    for (int i = 0; i < iterations; i++) {
        RegexMatches* matches = regex_findall(regex, text);
        if (matches) {
            found_count += matches->count;
            regex_matches_free(matches);
        }
    }
    double time = get_time_ms(start, clock());

    printf("  findall operations: %.2f ms\n", time);
    printf("  Total matches found: %d\n", found_count / iterations);

    regex_free(regex);
    printf("========================================\n\n");
}



int main() {
    printf("Performance Test: Our Regex Engine vs POSIX\n");
    printf("===========================================\n\n");

    printf("System: Linux\n");
    printf("\n");

    test_simple_patterns();
    test_catastrophic_backtracking();
    test_search_operations();

   
    printf("=== Performance Test Completed ===\n");
    return 0;
}
