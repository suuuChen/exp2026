#include "../include/ast.h"
/*#include "../include/ast.h"*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const char* pattern;
    size_t pos;
    size_t len;
    char current;
    char error[256];
} Parser;

static void parser_advance(Parser* p) {
    if (p->pos < p->len) {
        p->current = p->pattern[p->pos++];
    }
    else {
        p->current = '\0';
    }
}

static Parser* parser_new(const char* pattern) {
    Parser* p = malloc(sizeof(Parser));
    p->pattern = pattern;
    p->len = strlen(pattern);
    p->pos = 0;
    p->current = '\0';
    p->error[0] = '\0';
    parser_advance(p);
    return p;
}

static void parser_error(Parser* p, const char* msg) {
    snprintf(p->error, sizeof(p->error), "%s at position %zu", msg, p->pos);
}

static bool parser_match(Parser* p, char c) {
    if (p->current == c) {
        parser_advance(p);
        return true;
    }
    return false;
}

static AstNode* parse_char_class(Parser *p) {
    CharClass class = {0};
    class.negated = false;
    
    if (parser_match(p, '^')) {
        class.negated = true;
    }
    
    char *chars = NULL;
    size_t chars_len = 0;
    CharRange *ranges = NULL;  // 使用 CharRange 类型
    size_t ranges_len = 0;
    
    while (p->current != ']' && p->current != '\0') {
        if (p->current == '-' && chars_len > 0) {
            char start = chars[chars_len - 1];
            parser_advance(p);
            if (p->current != ']' && p->current != '\0') {
                char end = p->current;
                parser_advance(p);
                ranges = realloc(ranges, (ranges_len + 1) * sizeof(CharRange));
                ranges[ranges_len].start = start;
                ranges[ranges_len].end = end;
                ranges_len++;
                chars_len--;
            } else {
                chars = realloc(chars, chars_len + 1);
                chars[chars_len++] = '-';
            }
        } else {
            chars = realloc(chars, chars_len + 1);
            chars[chars_len++] = p->current;
            parser_advance(p);
        }
    }
    
    if (p->current != ']') {
        parser_error(p, "Expected ']'");
        free(chars);
        free(ranges);
        return NULL;
    }
    parser_advance(p);
    
    class.chars = chars;
    class.chars_len = chars_len;
    class.ranges = ranges;  // 现在类型匹配了
    class.ranges_len = ranges_len;
    
    return ast_new_char_class(&class);
}
static AstNode* parse_atom(Parser* p);
static AstNode* parse_expr(Parser* p);

static AstNode* parse_atom(Parser* p) {
    switch (p->current) {
    case '(': {
        parser_advance(p);
        AstNode* node = parse_expr(p);
        if (p->current != ')') {
            parser_error(p, "Expected ')'");
            ast_free(node);
            return NULL;
        }
        parser_advance(p);
        return ast_new_group(node);
    }
    case '[':
        parser_advance(p);
        return parse_char_class(p);
    case '^':
        parser_advance(p);
        return ast_new_char('\0');
    case '$':
        parser_advance(p);
        return ast_new_char('\0');
    case '.':
        parser_advance(p);
        return ast_new_any_char();
    case '\\': {
        parser_advance(p);
        char c = p->current;
        parser_advance(p);
        switch (c) {
        case 'd': return ast_new_char('\0');
        case 'w': return ast_new_char('\0');
        case 's': return ast_new_char('\0');
        default: return ast_new_char(c);
        }
    }
    default: {
        char c = p->current;
        parser_advance(p);
        return ast_new_char(c);
    }
    }
}

static AstNode* parse_repeat(Parser* p, AstNode* atom) {
    if (p->current == '*') {
        parser_advance(p);
        return ast_new_repeat(atom, 0, 0);
    }
    else if (p->current == '+') {
        parser_advance(p);
        return ast_new_repeat(atom, 1, 0);
    }
    else if (p->current == '?') {
        parser_advance(p);
        return ast_new_repeat(atom, 0, 1);
    }
    else if (p->current == '{') {
        parser_advance(p);
        size_t min = 0, max = 0;
        while (isdigit(p->current)) {
            min = min * 10 + (p->current - '0');
            parser_advance(p);
        }
        if (p->current == ',') {
            parser_advance(p);
            while (isdigit(p->current)) {
                max = max * 10 + (p->current - '0');
                parser_advance(p);
            }
        }
        else {
            max = min;
        }
        if (p->current != '}') {
            parser_error(p, "Expected '}'");
            ast_free(atom);
            return NULL;
        }
        parser_advance(p);
        return ast_new_repeat(atom, min, max);
    }
    return atom;
}

static AstNode* parse_factor(Parser* p) {
    AstNode* atom = parse_atom(p);
    if (!atom) return NULL;
    return parse_repeat(p, atom);
}

static AstNode* parse_term(Parser* p) {
    AstNode** children = NULL;
    size_t count = 0;

    while (p->current != '\0' && p->current != ')' && p->current != '|') {
        AstNode* factor = parse_factor(p);
        if (!factor) {
            for (size_t i = 0; i < count; i++) ast_free(children[i]);
            free(children);
            return NULL;
        }
        children = realloc(children, (count + 1) * sizeof(AstNode*));
        children[count++] = factor;
    }

    if (count == 0) return ast_new_char('\0');
    if (count == 1) return children[0];
    return ast_new_concat(children, count);
}

static AstNode* parse_expr(Parser* p) {
    AstNode** children = NULL;
    size_t count = 0;

    while (true) {
        AstNode* term = parse_term(p);
        if (!term) {
            for (size_t i = 0; i < count; i++) ast_free(children[i]);
            free(children);
            return NULL;
        }
        children = realloc(children, (count + 1) * sizeof(AstNode*));
        children[count++] = term;

        if (p->current != '|') break;
        parser_advance(p);
    }

    if (count == 0) return ast_new_char('\0');
    if (count == 1) return children[0];
    return ast_new_alternation(children, count);
}

AstNode* regex_parse(const char* pattern, char** error) {
    Parser* p = parser_new(pattern);
    AstNode* ast = parse_expr(p);

    if (p->error[0] != '\0' && error) {
        *error = strdup(p->error);
    }

    free(p);
    return ast;
}
