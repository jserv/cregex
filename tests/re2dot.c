#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cregex.h>

void printUsage(FILE *file, const char *program)
{
    fprintf(file, "usage: %s pattern\n", program);
}

void printNode(FILE *file, const cregex_node_t *node)
{
    switch (node->type) {
    case REGEX_NODE_TYPE_EPSILON:
        fprintf(file,
                "node%p[label=\"ε\",shape=box,fontname=\"times-italic\"];\n",
                (void *) node);
        break;

    /* Characters */
    case REGEX_NODE_TYPE_CHARACTER:
        fprintf(file,
                "node%p[color=lightblue2,style=filled,label=\"'%c'\",shape=box,"
                "fontname=\"courier\"];\n",
                (void *) node, node->ch);
        break;
    case REGEX_NODE_TYPE_ANY_CHARACTER:
        fprintf(file,
                "node%p[label=\"any\",shape=box"
                ",fontname=\"times-italic\"];\n",
                (void *) node);
        break;
    case REGEX_NODE_TYPE_CHARACTER_CLASS:
        fprintf(file,
                "node%p[label=\"[%.*s]\",shape=box,fontname=\"courier\"];\n",
                (void *) node, (int) (node->to - node->from), node->from);
        break;
    case REGEX_NODE_TYPE_CHARACTER_CLASS_NEGATED:
        fprintf(file,
                "node%p[label=\"[^%.*s]\",shape=box,fontname=\"courier\"];\n",
                (void *) node, (int) (node->to - node->from), node->from);
        break;

    /* Composites */
    case REGEX_NODE_TYPE_CONCATENATION:
        fprintf(file,
                "node%p[label=\"concatenation\",shape=box,style=\"rounded\""
                ",fontname=\"times-italic\"];\n",
                (void *) node);
        printNode(file, node->left);
        fprintf(file, "node%p->node%p;\n", (void *) node, (void *) node->left);
        printNode(file, node->right);
        fprintf(file, "node%p->node%p;\n", (void *) node, (void *) node->right);
        break;
    case REGEX_NODE_TYPE_ALTERNATION:
        fprintf(file,
                "node%p[label=\"alternation\",shape=diamond,style=\"rounded\""
                ",fontname=\"times-italic\"];\n",
                (void *) node);
        printNode(file, node->left);
        fprintf(file, "node%p->node%p;\n", (void *) node, (void *) node->left);
        printNode(file, node->right);
        fprintf(file, "node%p->node%p;\n", (void *) node, (void *) node->right);
        break;

    /* Quantifiers */
    case REGEX_NODE_TYPE_QUANTIFIER:
        fprintf(file, "node%p[label=\"%d..", (void *) node, node->nmin);
        if (node->nmax == -1)
            fprintf(file, "INF");
        else
            fprintf(file, "%d", node->nmax);
        if (node->nmin == 0)
            fprintf(file, "\",shape=ellipse,style=\"dotted\"];\n");
        else
            fprintf(file, "\",shape=ellipse];\n");
        printNode(file, node->quantified);
        fprintf(file, "node%p->node%p;\n", (void *) node,
                (void *) node->quantified);
        break;

    /* Anchors */
    case REGEX_NODE_TYPE_ANCHOR_BEGIN:
        fprintf(file, "node%p[label=\"^\",shape=circle];\n", (void *) node);
        break;
    case REGEX_NODE_TYPE_ANCHOR_END:
        fprintf(file, "node%p[label=\"$\",shape=circle];\n", (void *) node);
        break;

    /* Captures */
    case REGEX_NODE_TYPE_CAPTURE:
        fprintf(file,
                "node%p[label=\"capture\",shape=parallelogram,"
                "style=\"rounded\",fontname=\"times-italic\"];\n",
                (void *) node);
        printNode(file, node->captured);
        fprintf(file, "node%p->node%p;\n", (void *) node,
                (void *) node->captured);
        break;
    }
}

void printDot(FILE *file, const cregex_node_t *node)
{
    fprintf(file, "digraph cregex_ {\n");
    printNode(file, node);
    fprintf(file, "}\n");
}

int main(int argc, char *argv[])
{
    cregex_node_t *node;

    // process command line
    if (argc < 2 || argc > 3) {
        printUsage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printUsage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    // parse pattern
    if ((node = cregex_parse(argv[1])))
        printDot(stdout, node);
    else {
        fprintf(stderr, "%s: cregex_parse() failed\n", argv[0]);
        return EXIT_FAILURE;
    }

    cregex_parse_free(node);
    return EXIT_SUCCESS;
}
