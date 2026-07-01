#pragma once
#ifndef REGEX_ENGINE_H
#define REGEX_ENGINE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        REGEX_MODE_NFA,
        REGEX_MODE_DFA
    } RegexMode;

    typedef struct {
        size_t start;
        size_t end;
        size_t group_count;
        struct {
            size_t start;
            size_t end;
        } *groups;
    } RegexMatch;

    typedef struct Regex Regex;

    Regex* regex_compile(const char* pattern, RegexMode mode, const char** error);
    void regex_free(Regex* regex);
    bool regex_match(Regex* regex, const char* text, RegexMatch* match);
    bool regex_search(Regex* regex, const char* text, RegexMatch* match);

    typedef struct {
        RegexMatch* matches;
        size_t count;
        size_t capacity;
    } RegexMatches;

    RegexMatches* regex_findall(Regex* regex, const char* text);
    void regex_match_free(RegexMatch* match);
    void regex_matches_free(RegexMatches* matches);
    char* regex_get_transition_table(Regex* regex);
    char* regex_to_dot(Regex* regex);

#ifdef __cplusplus
}
#endif

#endif
