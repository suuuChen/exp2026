#include "../include/myregex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ===================== 测试用例结构 =====================

typedef struct {
    const char* pattern;
    const char* text;
    int expected_match;      // 1=应匹配, 0=不应匹配
    int expected_start;
    int expected_end;
    const char* description;
} TestCase;

// ===================== Grep 测试集 =====================

// 从 GNU Grep 测试集中提取的测试用例
static TestCase grep_test_suite[] = {
    // 基础字符匹配
    {"a", "a", 1, 0, 1, "单个字符匹配"},
    {"a", "b", 0, -1, -1, "单个字符不匹配"},
    {"abc", "abc", 1, 0, 3, "简单字符串匹配"},
    {"abc", "abx", 0, -1, -1, "简单字符串不匹配"},

    // 通配符 .
    {".", "a", 1, 0, 1, "点匹配任意字符"},
    {".", "\n", 1, 0, 1, "点匹配换行符"},
    {"a.c", "abc", 1, 0, 3, "点匹配中间字符"},
    {"a.c", "a\nc", 1, 0, 3, "点匹配换行"},

    // 重复 * + ?
    {"a*", "", 1, 0, 0, "星号匹配空字符串"},
    {"a*", "aaa", 1, 0, 3, "星号匹配多个"},
    {"a+", "", 0, -1, -1, "加号不匹配空"},
    {"a+", "a", 1, 0, 1, "加号匹配单个"},
    {"a+", "aaa", 1, 0, 3, "加号匹配多个"},
    {"a?", "", 1, 0, 0, "问号匹配空"},
    {"a?", "a", 1, 0, 1, "问号匹配单个"},
    {"a?", "aa", 1, 0, 1, "问号匹配单个（贪心）"},

    // 选择 |
    {"a|b", "a", 1, 0, 1, "选择匹配第一个"},
    {"a|b", "b", 1, 0, 1, "选择匹配第二个"},
    {"a|b", "c", 0, -1, -1, "选择不匹配"},
    {"(a|b|c)", "c", 1, 0, 1, "三选一"},

    // 分组 ()
    {"(ab)+", "ababab", 1, 0, 6, "分组重复"},
    {"(a|b)*c", "aaabbc", 1, 0, 6, "分组选择重复"},

    // 字符类 []
    {"[abc]", "a", 1, 0, 1, "字符类匹配"},
    {"[abc]", "d", 0, -1, -1, "字符类不匹配"},
    {"[0-9]", "5", 1, 0, 1, "字符范围匹配"},
    {"[0-9]", "a", 0, -1, -1, "字符范围不匹配"},
    {"[a-z]", "m", 1, 0, 1, "字母范围匹配"},
    {"[^0-9]", "a", 1, 0, 1, "取反字符类匹配"},
    {"[^0-9]", "5", 0, -1, -1, "取反字符类不匹配"},

    // 预定义字符类
    {"\\d", "5", 1, 0, 1, "数字匹配"},
    {"\\d", "a", 0, -1, -1, "数字不匹配"},
    {"\\d+", "123", 1, 0, 3, "多个数字"},
    {"\\w", "_", 1, 0, 1, "单词字符匹配"},
    {"\\w", "!", 0, -1, -1, "单词字符不匹配"},
    {"\\s", " ", 1, 0, 1, "空白匹配"},
    {"\\s", "a", 0, -1, -1, "空白不匹配"},

    // 锚点 ^ $
    {"^a", "a", 1, 0, 1, "行首匹配"},
    {"^a", "ba", 0, -1, -1, "行首不匹配"},
    {"a$", "a", 1, 0, 1, "行尾匹配"},
    {"a$", "ab", 0, -1, -1, "行尾不匹配"},
    {"^abc$", "abc", 1, 0, 3, "完全匹配"},

    // 精确重复 {m,n}
    {"a{2}", "aa", 1, 0, 2, "精确2次"},
    {"a{2}", "a", 0, -1, -1, "精确2次不匹配"},
    {"a{2,4}", "aa", 1, 0, 2, "2-4次最小"},
    {"a{2,4}", "aaaa", 1, 0, 4, "2-4次最大"},
    {"a{2,4}", "aaaaa", 1, 0, 4, "2-4次贪心"},
    {"a{2,}", "aaa", 1, 0, 3, "至少2次"},

    // 复杂组合
    {"(a|b)*abb", "abb", 1, 0, 3, "组合1"},
    {"(a|b)*abb", "aaabb", 1, 0, 5, "组合2"},
    {"(a|b)*abb", "ababb", 1, 0, 5, "组合3"},
    {"[0-9]+\\.[0-9]+", "3.14", 1, 0, 4, "浮点数"},
    {"[a-zA-Z]+", "Hello", 1, 0, 5, "字母串"},

    // 边界用例
    {"", "", 1, 0, 0, "空模式匹配空字符串"},
    {"", "abc", 1, 0, 0, "空模式匹配任意字符串"},
    {"a*", "b", 1, 0, 0, "星号匹配零次"},
    {"a*$", "b", 1, 0, 0, "行尾匹配"},
    {"^$", "", 1, 0, 0, "空行"},
};

#define TEST_COUNT (sizeof(grep_test_suite) / sizeof(TestCase))

// ===================== 测试运行器 =====================

typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
    char error_msg[256];
} TestResult;

static void run_test_case(TestCase* tc, RegexMode mode, TestResult* result) {
    result->total++;

    const char* error = NULL;
    Regex* regex = regex_compile(tc->pattern, mode, &error);

    if (!regex) {
        result->failed++;
        printf("  ❌ %s: 编译失败 - %s\n", tc->description, error ? error : "unknown");
        return;
    }

    RegexMatch match;
    bool matched = regex_match(regex, tc->text, &match);

    bool success = (matched == (tc->expected_match == 1));
    if (success && matched) {
        if (tc->expected_start >= 0) {
            success = (match.start == (size_t)tc->expected_start &&
                match.end == (size_t)tc->expected_end);
        }
    }

    if (success) {
        result->passed++;
        printf("  ✅ %s: 通过\n", tc->description);
    }
    else {
        result->failed++;
        printf("  ❌ %s: 失败 - ", tc->description);
        printf("期望 %s", tc->expected_match ? "匹配" : "不匹配");
        if (matched) {
            printf(", 实际匹配 [%zu, %zu)", match.start, match.end);
        }
        else {
            printf(", 实际不匹配");
        }
        printf("\n");
    }

    regex_free(regex);
}

static void run_test_suite(RegexMode mode, const char* mode_name) {
    TestResult result = { 0 };

    printf("\n========== 测试模式: %s ==========\n", mode_name);
    printf("总用例数: %zu\n", TEST_COUNT);
    printf("\n");

    for (size_t i = 0; i < TEST_COUNT; i++) {
        run_test_case(&grep_test_suite[i], mode, &result);
    }

    printf("\n---------- 测试结果 ----------\n");
    printf("总用例: %d\n", result.total);
    printf("通过: %d\n", result.passed);
    printf("失败: %d\n", result.failed);

    double pass_rate = (double)result.passed / result.total * 100.0;
    printf("通过率: %.2f%%\n", pass_rate);

    if (pass_rate >= 90.0) {
        printf("✅ 通过率 >= 90%%，验收通过！\n");
    }
    else {
        printf("⚠️ 通过率 < 90%%，需要改进\n");
    }
}

// ===================== 主函数 =====================

int main() {
    printf("========================================\n");
    printf("  正则表达式引擎 - Grep 测试集验收\n");
    printf("========================================\n");

    // 测试 NFA 模式
    run_test_suite(REGEX_MODE_NFA, "NFA");

    // 测试 DFA 模式
    run_test_suite(REGEX_MODE_DFA, "DFA");

    printf("\n========================================\n");
    printf("  测试完成\n");
    printf("========================================\n");

    return 0;
}

