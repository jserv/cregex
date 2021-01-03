#include <stddef.h>
#include <stdlib.h>

#include "cregex.h"

typedef struct {
    cregex_program_instr_t *pc;
    int ncaptures;
} cregex_compileContext;

static int cregex_compileCountInstructions(const cregex_node_t *node)
{
    int ninstructions;

    switch (node->type) {
    case REGEX_NODE_TYPE_EPSILON:
        return 0;

    /* Characters */
    case REGEX_NODE_TYPE_CHARACTER:
    case REGEX_NODE_TYPE_ANY_CHARACTER:
    case REGEX_NODE_TYPE_CHARACTER_CLASS:
    case REGEX_NODE_TYPE_CHARACTER_CLASS_NEGATED:
        return 1;

    /* Composites */
    case REGEX_NODE_TYPE_CONCATENATION:
        return cregex_compileCountInstructions(node->left) +
               cregex_compileCountInstructions(node->right);
    case REGEX_NODE_TYPE_ALTERNATION:
        return 2 + cregex_compileCountInstructions(node->left) +
               cregex_compileCountInstructions(node->right);

    /* Quantifiers */
    case REGEX_NODE_TYPE_QUANTIFIER:
        ninstructions = cregex_compileCountInstructions(node->quantified);
        if (node->nmax >= node->nmin)
            return node->nmin * ninstructions +
                   (node->nmax - node->nmin) * (ninstructions + 1);
        return 1 +
               (node->nmin ? node->nmin * ninstructions : ninstructions + 1);

    /* Anchors */
    case REGEX_NODE_TYPE_ANCHOR_BEGIN:
    case REGEX_NODE_TYPE_ANCHOR_END:
        return 1;

    /* Captures */
    case REGEX_NODE_TYPE_CAPTURE:
        return 2 + cregex_compileCountInstructions(node->captured);
    }
}

static int cregex_compile_nodeIsAnchored(const cregex_node_t *node)
{
    switch (node->type) {
    case REGEX_NODE_TYPE_EPSILON:
        return 0;

    /* Characters */
    case REGEX_NODE_TYPE_CHARACTER:
    case REGEX_NODE_TYPE_ANY_CHARACTER:
    case REGEX_NODE_TYPE_CHARACTER_CLASS:
    case REGEX_NODE_TYPE_CHARACTER_CLASS_NEGATED:
        return 0;

    /* Composites */
    case REGEX_NODE_TYPE_CONCATENATION:
        return cregex_compile_nodeIsAnchored(node->left);
    case REGEX_NODE_TYPE_ALTERNATION:
        return cregex_compile_nodeIsAnchored(node->left) &&
               cregex_compile_nodeIsAnchored(node->right);

    /* Quantifiers */
    case REGEX_NODE_TYPE_QUANTIFIER:
        return cregex_compile_nodeIsAnchored(node->quantified);

    /* Anchors */
    case REGEX_NODE_TYPE_ANCHOR_BEGIN:
        return 1;
    case REGEX_NODE_TYPE_ANCHOR_END:
        return 0;

    /* Captures */
    case REGEX_NODE_TYPE_CAPTURE:
        return cregex_compile_nodeIsAnchored(node->captured);
    }
}

static inline cregex_program_instr_t *cregex_compileEmit(
    cregex_compileContext *context,
    const cregex_program_instr_t *instruction)
{
    *context->pc = *instruction;
    return context->pc++;
}

static cregex_program_instr_t *cregex_compileCharacterClass(
    const cregex_node_t *node,
    cregex_program_instr_t *instruction)
{
    const char *sp = node->from;

    for (;;) {
        int ch = *sp++;
        switch (ch) {
        case ']':
            if (sp - 1 == node->from)
                goto CHARACTER;
            return instruction;
        case '\\':
            ch = *sp++;
            /* fall-through */
        default:
        CHARACTER:
            if (*sp == '-' && sp[1] != ']') {
                for (; ch <= sp[1]; ++ch)
                    cregex_char_class_add(instruction->klass, ch);
                sp += 2;
            } else {
                cregex_char_class_add(instruction->klass, ch);
            }
            break;
        }
    }
}

static cregex_program_instr_t *compile_context(cregex_compileContext *context,
                                               const cregex_node_t *node)
{
    cregex_program_instr_t *bottom = context->pc, *split, *jump, *last;
    int ncaptures = context->ncaptures, capture;

    switch (node->type) {
    case REGEX_NODE_TYPE_EPSILON:
        break;

    /* Characters */
    case REGEX_NODE_TYPE_CHARACTER:
        cregex_compileEmit(
            context,
            &(cregex_program_instr_t){.opcode = REGEX_PROGRAM_OPCODE_CHARACTER,
                                      .ch = node->ch});
        break;
    case REGEX_NODE_TYPE_ANY_CHARACTER:
        cregex_compileEmit(context,
                           &(cregex_program_instr_t){
                               .opcode = REGEX_PROGRAM_OPCODE_ANY_CHARACTER});
        break;
    case REGEX_NODE_TYPE_CHARACTER_CLASS:
        cregex_compileCharacterClass(
            node,
            cregex_compileEmit(
                context, &(cregex_program_instr_t){
                             .opcode = REGEX_PROGRAM_OPCODE_CHARACTER_CLASS}));
        break;
    case REGEX_NODE_TYPE_CHARACTER_CLASS_NEGATED:
        cregex_compileCharacterClass(
            node,
            cregex_compileEmit(
                context,
                &(cregex_program_instr_t){
                    .opcode = REGEX_PROGRAM_OPCODE_CHARACTER_CLASS_NEGATED}));
        break;

    /* Composites */
    case REGEX_NODE_TYPE_CONCATENATION:
        compile_context(context, node->left);
        compile_context(context, node->right);
        break;
    case REGEX_NODE_TYPE_ALTERNATION:
        split = cregex_compileEmit(
            context,
            &(cregex_program_instr_t){.opcode = REGEX_PROGRAM_OPCODE_SPLIT});
        split->first = compile_context(context, node->left);
        jump = cregex_compileEmit(
            context,
            &(cregex_program_instr_t){.opcode = REGEX_PROGRAM_OPCODE_JUMP});
        split->second = compile_context(context, node->right);
        jump->target = context->pc;
        break;

    /* Quantifiers */
    case REGEX_NODE_TYPE_QUANTIFIER:
        for (int i = 0; i < node->nmin; ++i) {
            context->ncaptures = ncaptures;
            last = compile_context(context, node->quantified);
        }
        if (node->nmax > node->nmin) {
            for (int i = 0; i < node->nmax - node->nmin; ++i) {
                context->ncaptures = ncaptures;
                split = cregex_compileEmit(
                    context, &(cregex_program_instr_t){
                                 .opcode = REGEX_PROGRAM_OPCODE_SPLIT});
                split->first = compile_context(context, node->quantified);
                split->second = context->pc;
                if (!node->greedy) {
                    cregex_program_instr_t *swap = split->first;
                    split->first = split->second;
                    split->second = swap;
                }
            }
        } else if (node->nmax == -1) {
            split = cregex_compileEmit(
                context, &(cregex_program_instr_t){
                             .opcode = REGEX_PROGRAM_OPCODE_SPLIT});
            if (node->nmin == 0) {
                split->first = compile_context(context, node->quantified);
                jump = cregex_compileEmit(
                    context, &(cregex_program_instr_t){
                                 .opcode = REGEX_PROGRAM_OPCODE_JUMP});
                split->second = context->pc;
                jump->target = split;
            } else {
                split->first = last;
                split->second = context->pc;
            }
            if (!node->greedy) {
                cregex_program_instr_t *swap = split->first;
                split->first = split->second;
                split->second = swap;
            }
        }
        break;

    /* Anchors */
    case REGEX_NODE_TYPE_ANCHOR_BEGIN:
        cregex_compileEmit(context,
                           &(cregex_program_instr_t){
                               .opcode = REGEX_PROGRAM_OPCODE_ASSERT_BEGIN});
        break;
    case REGEX_NODE_TYPE_ANCHOR_END:
        cregex_compileEmit(context,
                           &(cregex_program_instr_t){
                               .opcode = REGEX_PROGRAM_OPCODE_ASSERT_END});
        break;

    /* Captures */
    case REGEX_NODE_TYPE_CAPTURE:
        capture = context->ncaptures++ * 2;
        cregex_compileEmit(
            context, &(cregex_program_instr_t){
                         .opcode = REGEX_PROGRAM_OPCODE_SAVE, .save = capture});
        compile_context(context, node->captured);
        cregex_compileEmit(context, &(cregex_program_instr_t){
                                        .opcode = REGEX_PROGRAM_OPCODE_SAVE,
                                        .save = capture + 1});
        break;
    }

    return bottom;
}

cregex_program_t *cregex_compile(const char *pattern)
{
    cregex_node_t *root;
    cregex_program_t *program;

    if (!(root = cregex_parse(pattern)))
        return NULL;

    program = cregex_compile_node(root);
    cregex_parse_free(root);
    return program;
}

/* Compile a parsed pattern (using a previously allocated program with at least
 * cregex_compileEstimateInstructions(root) instructions).
 */
static cregex_program_t *cregex_compile_nodeWithProgram(
    const cregex_node_t *root,
    cregex_program_t *program)
{
    // add capture node for entire match
    root = &(cregex_node_t){.type = REGEX_NODE_TYPE_CAPTURE,
                            .captured = (cregex_node_t *) root};

    // add .*? unless pattern starts with ^
    if (!cregex_compile_nodeIsAnchored(root))
        root = &(cregex_node_t){
            .type = REGEX_NODE_TYPE_CONCATENATION,
            .left =
                &(cregex_node_t){
                    .type = REGEX_NODE_TYPE_QUANTIFIER,
                    .nmin = 0,
                    .nmax = -1,
                    .greedy = 0,
                    .quantified = &(
                        cregex_node_t){.type = REGEX_NODE_TYPE_ANY_CHARACTER}},
            .right = (cregex_node_t *) root};

    /* compile */
    cregex_compileContext *context =
        &(cregex_compileContext){.pc = program->instructions, .ncaptures = 0};
    compile_context(context, root);

    /* emit final match instruction */
    cregex_compileEmit(context, &(cregex_program_instr_t){
                                    .opcode = REGEX_PROGRAM_OPCODE_MATCH});

    /* set total number of instructions */
    program->ninstructions = context->pc - program->instructions;

    return program;
}

/* Upper bound of number of instructions required to compile parsed pattern. */
static int cregex_compileEstimateInstructions(const cregex_node_t *root)
{
    return cregex_compileCountInstructions(root)
           /* .*? is added unless pattern starts with ^,
            * save instructions are added for beginning and end of match,
            * a final match instruction is added to the end of the program */
           + !cregex_compile_nodeIsAnchored(root) * 3 + 2 + 1;
}

cregex_program_t *cregex_compile_node(const cregex_node_t *root)
{
    size_t size =
        sizeof(cregex_program_t) + sizeof(cregex_program_instr_t) *
                                       cregex_compileEstimateInstructions(root);
    cregex_program_t *program;

    if (!(program = malloc(size)))
        return NULL;

    if (!cregex_compile_nodeWithProgram(root, program)) {
        free(program);
        return NULL;
    }

    return program;
}

/* Free a compiled program */
void cregex_compile_free(cregex_program_t *program)
{
    free(program);
}
