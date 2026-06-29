#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    AST_EMPTY,
    AST_CHAR,
    AST_ANY_CHAR,
    AST_START,
    AST_END,
    AST_DIGIT,
    AST_WORD,
    AST_WHITESPACE,
    AST_CONCAT,
    AST_ALTERNATION,
    AST_REPEAT,
    AST_GROUP,
    AST_CHAR_CLASS
} AstType;

// 定义字符范围结构体
typedef struct CharRange {
    char start;
    char end;
} CharRange;

typedef struct {
    bool negated;
    char *chars;
    size_t chars_len;
    CharRange *ranges;
    size_t ranges_len;
} CharClass;

typedef struct AstNode {
    AstType type;
    union {
        char ch;
        CharClass char_class;
        struct {
            struct AstNode **children;
            size_t child_count;
        } concat;
        struct {
            struct AstNode **children;
            size_t child_count;
        } alternation;
        struct {
            struct AstNode *child;
            size_t min;
            size_t max;
        } repeat;
        struct {
            struct AstNode *child;
        } group;
    };
} AstNode;

AstNode* ast_new_char(char c);
AstNode* ast_new_any_char(void);
AstNode* ast_new_concat(AstNode **children, size_t count);
AstNode* ast_new_alternation(AstNode **children, size_t count);
AstNode* ast_new_repeat(AstNode *child, size_t min, size_t max);
AstNode* ast_new_group(AstNode *child);
AstNode* ast_new_char_class(CharClass *char_class);
void ast_free(AstNode *node);

#endif
