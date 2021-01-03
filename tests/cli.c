#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cregex.h>

void printUsage(FILE *file, const char *program)
{
    fprintf(file, "usage: %s pattern [string...]\n", program);
}

void printNode(FILE *file, cregex_node_t *node, int depth)
{
    switch (node->type) {
    case REGEX_NODE_TYPE_EPSILON:
        fprintf(file, "epsilon");
        break;

    /* Characters */
    case REGEX_NODE_TYPE_CHARACTER:
        fprintf(file, isprint(node->ch) ? "character('%c')" : "character(%02x)",
                node->ch);
        break;
    case REGEX_NODE_TYPE_ANY_CHARACTER:
        fprintf(file, "any_character");
        break;
    case REGEX_NODE_TYPE_CHARACTER_CLASS:
        fprintf(file, "character_class(\"%.*s\")",
                (int) (node->to - node->from), node->from);
        break;
    case REGEX_NODE_TYPE_CHARACTER_CLASS_NEGATED:
        fprintf(file, "character_class_negated(\"%.*s\")",
                (int) (node->to - node->from), node->from);
        break;

    /* Composites */
    case REGEX_NODE_TYPE_CONCATENATION:
        fprintf(file, "concatenation(");
        printNode(file, node->left, depth + 1);
        fprintf(file, ", ");
        printNode(file, node->right, depth + 1);
        fprintf(file, ")");
        break;
    case REGEX_NODE_TYPE_ALTERNATION:
        fprintf(file, "alternation(");
        printNode(file, node->left, depth + 1);
        fprintf(file, ", ");
        printNode(file, node->right, depth + 1);
        fprintf(file, ")");
        break;

    /* Quantifiers */
    case REGEX_NODE_TYPE_QUANTIFIER:
        fprintf(file, "quantifier(");
        printNode(file, node->quantified, depth + 1);
        fprintf(file, ", %d, %d, %s)", node->nmin, node->nmax,
                node->greedy ? "greedy" : "non_greedy");
        break;

    /* Anchors */
    case REGEX_NODE_TYPE_ANCHOR_BEGIN:
        fprintf(file, "anchor_begin");
        break;
    case REGEX_NODE_TYPE_ANCHOR_END:
        fprintf(file, "anchor_end");
        break;

    /* Captures */
    case REGEX_NODE_TYPE_CAPTURE:
        fprintf(file, "capture(");
        printNode(file, node->captured, depth + 1);
        fprintf(file, ")");
        break;
    }

    if (depth == 0)
        fprintf(file, "\n");
}

void printCharacterClass(FILE *file, const cregex_program_instr_t *instruction)
{
    for (int ch = 0, to; ch < UCHAR_MAX; ++ch) {
        if (cregex_char_class_contains(instruction->klass, ch)) {
            fprintf(file, isprint(ch) ? "%c" : "%02x", ch);
            for (to = ch + 1;
                 cregex_char_class_contains(instruction->klass, to); ++to)
                ;
            if (to > ch + 2) {
                fprintf(file, isprint(to) ? "-%c" : "-%02x", to - 1);
                ch = to;
            }
        }
    }
}

void printInstruction(FILE *file,
                      const cregex_program_t *program,
                      const cregex_program_instr_t *instruction)
{
    fprintf(file, "[%04x] ", (int) (instruction - program->instructions));

    switch (instruction->opcode) {
    case REGEX_PROGRAM_OPCODE_MATCH:
        fprintf(file, "MATCH\n");
        break;

    /* Characters */
    case REGEX_PROGRAM_OPCODE_CHARACTER:
        if (isprint(instruction->ch))
            fprintf(file, "CHAR %c\n", instruction->ch);
        else
            fprintf(file, "CHAR %02x\n", instruction->ch);
        break;
    case REGEX_PROGRAM_OPCODE_ANY_CHARACTER:
        fprintf(file, "ANY_CHAR\n");
        break;
    case REGEX_PROGRAM_OPCODE_CHARACTER_CLASS:
        fprintf(file, "CHARACTER_CLASS [");
        printCharacterClass(file, instruction);
        fprintf(file, "]\n");
        break;
    case REGEX_PROGRAM_OPCODE_CHARACTER_CLASS_NEGATED:
        fprintf(file, "CHARACTER_CLASS_NEGATED [^");
        printCharacterClass(file, instruction);
        fprintf(file, "]\n");
        break;

    /* Control-flow */
    case REGEX_PROGRAM_OPCODE_JUMP:
        fprintf(file, "JUMP %04x\n",
                (int) (instruction->target - program->instructions));
        break;
    case REGEX_PROGRAM_OPCODE_SPLIT:
        fprintf(file, "SPLIT %04x %04x\n",
                (int) (instruction->first - program->instructions),
                (int) (instruction->second - program->instructions));
        break;

    /* Assertions */
    case REGEX_PROGRAM_OPCODE_ASSERT_BEGIN:
        fprintf(file, "ASSERT_BEGIN\n");
        break;
    case REGEX_PROGRAM_OPCODE_ASSERT_END:
        fprintf(file, "ASSERT_END\n");
        break;

    /* Saving */
    case REGEX_PROGRAM_OPCODE_SAVE:
        fprintf(file, "SAVE %d\n", instruction->save);
        break;
    }
}

void printProgram(FILE *file, const cregex_program_t *program)
{
    for (int i = 0; i < program->ninstructions; ++i)
        printInstruction(file, program, program->instructions + i);
}

int main(int argc, char *argv[])
{
    cregex_node_t *node;
    cregex_program_t *program;

    // process command line
    if (argc < 2) {
        printUsage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printUsage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    // parse pattern
    if ((node = cregex_parse(argv[1])))
        printNode(stdout, node, 0);
    else {
        fprintf(stderr, "%s: cregex_parse() failed\n", argv[0]);
        return EXIT_FAILURE;
    }

    // compile parsed pattern
    program = cregex_compile_node(node);
    cregex_parse_free(node);
    if (program)
        printProgram(stdout, program);
    else {
        fprintf(stderr, "%s: cregex_compile_node() failed\n", argv[0]);
        return EXIT_FAILURE;
    }

    // run program on string(s)
    for (int i = 2; i < argc; ++i) {
        const char *matches[20] = {0};
        int nmatches = 0;

        if (cregex_program_run(program, argv[i], matches, 20) > 0) {
            for (int j = 0; j < sizeof(matches) / sizeof(matches[0]); ++j)
                if (matches[j])
                    nmatches = j;

            printf("\"%s\": ", argv[i]);

            for (int j = 0; j <= nmatches; j += 2) {
                if (j > 0)
                    printf(", ");
                if (matches[j] && matches[j + 1]) {
                    printf("\"%.*s\"(%d,%d)",
                           (int) (matches[j + 1] - matches[j]), matches[j],
                           (int) (matches[j] - argv[i]),
                           (int) (matches[j + 1] - argv[i]));
                } else {
                    printf("(NULL,NULL)");
                }
            }

            printf("\n");
        } else {
            printf("\"%s\": no match\n", argv[i]);
        }
    }

    cregex_compile_free(program);
    return EXIT_SUCCESS;
}
