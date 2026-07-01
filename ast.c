#include "../include/ast.h"
#include <stdlib.h>
#include <string.h>

AstNode* ast_new_char(char c) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_CHAR;
    node->ch = c;
    return node;
}

AstNode* ast_new_any_char(void) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_ANY_CHAR;
    return node;
}

AstNode* ast_new_concat(AstNode** children, size_t count) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_CONCAT;
    node->concat.children = children;
    node->concat.child_count = count;
    return node;
}

AstNode* ast_new_alternation(AstNode** children, size_t count) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_ALTERNATION;
    node->alternation.children = children;
    node->alternation.child_count = count;
    return node;
}

AstNode* ast_new_repeat(AstNode* child, size_t min, size_t max) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_REPEAT;
    node->repeat.child = child;
    node->repeat.min = min;
    node->repeat.max = max;
    return node;
}

AstNode* ast_new_group(AstNode* child) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_GROUP;
    node->group.child = child;
    return node;
}

AstNode* ast_new_char_class(CharClass* char_class) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_CHAR_CLASS;
    node->char_class = *char_class;
    return node;
}

// 新增锚点、预定义类节点实现
AstNode* ast_new_start(void) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_START;
    return node;
}

AstNode* ast_new_end(void) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_END;
    return node;
}

AstNode* ast_new_digit(void) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_DIGIT;
    return node;
}

AstNode* ast_new_word(void) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_WORD;
    return node;
}

AstNode* ast_new_whitespace(void) {
    AstNode* node = calloc(1, sizeof(AstNode));
    node->type = AST_WHITESPACE;
    return node;
}

void ast_free(AstNode* node) {
    if (!node) return;

    switch (node->type) {
    case AST_CONCAT:
        for (size_t i = 0; i < node->concat.child_count; i++) {
            ast_free(node->concat.children[i]);
        }
        free(node->concat.children);
        break;
    case AST_ALTERNATION:
        for (size_t i = 0; i < node->alternation.child_count; i++) {
            ast_free(node->alternation.children[i]);
        }
        free(node->alternation.children);
        break;
    case AST_REPEAT:
        ast_free(node->repeat.child);
        break;
    case AST_GROUP:
        ast_free(node->group.child);
        break;
    case AST_CHAR_CLASS:
        free(node->char_class.chars);
        free(node->char_class.ranges);
        break;
    default:
        break;
    }
    free(node);
}
