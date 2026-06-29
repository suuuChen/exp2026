#include "../include/regex.h"
#include "../include/nfa.h"
#include <stdio.h>

int main() {
    const char* pattern = "a+b";
    printf("Pattern: %s\n", pattern);

    // 直接构建 NFA
    char* error = NULL;
    AstNode* ast = regex_parse(pattern, &error);
    if (!ast) {
        printf("Parse error: %s\n", error);
        return 1;
    }

    NFA* nfa = nfa_from_ast(ast);
    printf("\nNFA built:\n");
    printf("Start state: %zu\n", nfa->start_state);
    printf("Accept state: %zu\n", nfa->accept_state);
    printf("Total states: %zu\n", nfa->state_count);
    printf("Total edges: %zu\n", nfa->edge_count);

    printf("\nTransition table:\n");
    nfa_print_transition_table(nfa);

    ast_free(ast);
    nfa_free(nfa);

    return 0;
}