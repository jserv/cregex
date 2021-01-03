#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cregex.h"

typedef struct {
    const char *sp;
    cregex_node_t *stack, *output;
} cregex_parseContext;

/* Shunting-yard algorithm
 * See https://en.wikipedia.org/wiki/Shunting-yard_algorithm
 */

static inline cregex_node_t *push(cregex_parseContext *context,
                                  const cregex_node_t *node)
{
    assert(context->stack <= context->output);
    *context->stack = *node;
    return context->stack++;
}

static inline cregex_node_t *drop(cregex_parseContext *context)
{
    return --context->stack;
}

static inline cregex_node_t *consume(cregex_parseContext *context)
{
    *--context->output = *--context->stack;
    return context->output;
}

static inline cregex_node_t *concatenate(cregex_parseContext *context,
                                         const cregex_node_t *bottom)
{
    if (context->stack == bottom)
        push(context, &(cregex_node_t){.type = REGEX_NODE_TYPE_EPSILON});
    else {
        while (context->stack - 1 > bottom) {
            cregex_node_t *right = consume(context);
            cregex_node_t *left = consume(context);
            push(context,
                 &(cregex_node_t){.type = REGEX_NODE_TYPE_CONCATENATION,
                                  .left = left,
                                  .right = right});
        }
    }
    return context->stack - 1;
}

static cregex_node_t *parse_char_class(cregex_parseContext *context)
{
    cregex_node_type type =
        (*context->sp == '^')
            ? (++context->sp, REGEX_NODE_TYPE_CHARACTER_CLASS_NEGATED)
            : REGEX_NODE_TYPE_CHARACTER_CLASS;
    const char *from = context->sp;

    for (;;) {
        int ch = *context->sp++;
        switch (ch) {
        case '\0':
            /* premature end of character class */
            return NULL;
        case ']':
            if (context->sp - 1 == from)
                goto CHARACTER;
            return push(context,
                        &(cregex_node_t){
                            .type = type, .from = from, .to = context->sp - 1});
        case '\\':
            ch = *context->sp++;
            /* fall-through */
        default:
        CHARACTER:
            if (*context->sp == '-' && context->sp[1] != ']') {
                if (context->sp[1] < ch)
                    /* empty range in character class */
                    return NULL;
                context->sp += 2;
            }
            break;
        }
    }
}

static cregex_node_t *parse_interval(cregex_parseContext *context)
{
    const char *from = context->sp;
    int nmin, nmax;

    for (nmin = 0; *context->sp >= '0' && *context->sp <= '9'; ++context->sp)
        nmin = (nmin * 10) + (*context->sp - '0');

    if (*context->sp == ',') {
        ++context->sp;
        if (*from != ',' && *context->sp == '}')
            nmax = -1;
        else {
            for (nmax = 0; *context->sp >= '0' && *context->sp <= '9';
                 ++context->sp)
                nmax = (nmax * 10) + (*context->sp - '0');
            if (*(context->sp - 1) == ',' || *context->sp != '}' ||
                nmax < nmin) {
                context->sp = from;
                return NULL;
            }
        }
    } else if (*from != '}' && *context->sp == '}') {
        nmax = nmin;
    } else {
        context->sp = from;
        return NULL;
    }

    ++context->sp;
    return push(context,
                &(cregex_node_t){
                    .type = REGEX_NODE_TYPE_QUANTIFIER,
                    .nmin = nmin,
                    .nmax = nmax,
                    .greedy = (*context->sp == '?') ? (++context->sp, 0) : 1,
                    .quantified = consume(context)});
}

static cregex_node_t *parse_context(cregex_parseContext *context, int depth)
{
    cregex_node_t *bottom = context->stack;

    for (;;) {
        cregex_node_t *left, *right;
        int ch = *context->sp++;
        switch (ch) {
        /* Characters */
        case '\\':
            ch = *context->sp++;
            /* fall-through */
        default:
        CHARACTER:
            push(context,
                 &(cregex_node_t){.type = REGEX_NODE_TYPE_CHARACTER, .ch = ch});
            break;
        case '.':
            push(context,
                 &(cregex_node_t){.type = REGEX_NODE_TYPE_ANY_CHARACTER});
            break;
        case '[':
            if (!parse_char_class(context))
                return NULL;
            break;

        /* Composites */
        case '|':
            left = concatenate(context, bottom);
            if (!(right = parse_context(context, depth)))
                return NULL;
            if (left->type == REGEX_NODE_TYPE_EPSILON &&
                right->type == left->type) {
                drop(context);
            } else if (left->type == REGEX_NODE_TYPE_EPSILON) {
                right = consume(context);
                drop(context);
                push(context,
                     &(cregex_node_t){.type = REGEX_NODE_TYPE_QUANTIFIER,
                                      .nmin = 0,
                                      .nmax = 1,
                                      .greedy = 1,
                                      .quantified = right});
            } else if (right->type == REGEX_NODE_TYPE_EPSILON) {
                drop(context);
                left = consume(context);
                push(context,
                     &(cregex_node_t){.type = REGEX_NODE_TYPE_QUANTIFIER,
                                      .nmin = 0,
                                      .nmax = 1,
                                      .greedy = 1,
                                      .quantified = left});
            } else {
                right = consume(context);
                left = consume(context);
                push(context,
                     &(cregex_node_t){.type = REGEX_NODE_TYPE_ALTERNATION,
                                      .left = left,
                                      .right = right});
            }
            return bottom;

            /* Quantifiers */
#define QUANTIFIER(ch, min, max)                                           \
    case ch:                                                               \
        if (context->stack == bottom)                                      \
            goto CHARACTER;                                                \
        push(context,                                                      \
             &(cregex_node_t){                                             \
                 .type = REGEX_NODE_TYPE_QUANTIFIER,                       \
                 .nmin = min,                                              \
                 .nmax = max,                                              \
                 .greedy = (*context->sp == '?') ? (++context->sp, 0) : 1, \
                 .quantified = consume(context)});                         \
        break

            QUANTIFIER('?', 0, 1);
            QUANTIFIER('*', 0, -1);
            QUANTIFIER('+', 1, -1);
#undef QUANTIFIER

        case '{':
            if ((context->stack == bottom) || !parse_interval(context))
                goto CHARACTER;
            break;

        /* Anchors */
        case '^':
            push(context,
                 &(cregex_node_t){.type = REGEX_NODE_TYPE_ANCHOR_BEGIN});
            break;
        case '$':
            push(context, &(cregex_node_t){.type = REGEX_NODE_TYPE_ANCHOR_END});
            break;

        /* Captures */
        case '(':
            if (!parse_context(context, depth + 1))
                return NULL;
            push(context, &(cregex_node_t){.type = REGEX_NODE_TYPE_CAPTURE,
                                           .captured = consume(context)});
            break;
        case ')':
            if (depth > 0)
                return concatenate(context, bottom);
            /* unmatched close parenthesis */
            return NULL;

        /* End of string */
        case '\0':
            if (depth == 0)
                return concatenate(context, bottom);
            /* unmatched open parenthesis */
            return NULL;
        }
    }
}

static int parse_estimate_nodes(const char *pattern)
{
    return strlen(pattern) * 2;
}

/* Parse a pattern (using a previously allocated buffer of at least
 * parse_estimate_nodes(pattern) nodes).
 */
static cregex_node_t *parse_with_nodes(const char *pattern,
                                       cregex_node_t *nodes)
{
    cregex_parseContext *context =
        &(cregex_parseContext){.sp = pattern,
                               .stack = nodes,
                               .output = nodes + parse_estimate_nodes(pattern)};
    return parse_context(context, 0);
}

cregex_node_t *cregex_parse(const char *pattern)
{
    size_t size = sizeof(cregex_node_t) * parse_estimate_nodes(pattern);
    cregex_node_t *nodes;

    if (!(nodes = malloc(size)))
        return NULL;

    if (!parse_with_nodes(pattern, nodes)) {
        free(nodes);
        return NULL;
    }

    return nodes;
}

void cregex_parse_free(cregex_node_t *root)
{
    free(root);
}
