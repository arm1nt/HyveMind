#ifndef _HYVEMIND_LIB_RADIX_TREE_H
#define _HYVEMIND_LIB_RADIX_TREE_H

#include "hyvstdlib.h"

#define RADIX_MAX_NR_CHILDREN 256

struct radix_node {
    char *prefix;
    struct radix_node *children[RADIX_MAX_NR_CHILDREN];
    uintptr_t data;
    bool terminating;
};

struct radix_tree {
    struct radix_node *root;
};

struct radix_tree *create_radix_tree(void);
bool radix_tree_add(struct radix_tree *tree, const char *key, const uintptr_t data);
bool radix_tree_contains(const struct radix_tree *tree, const char *key);
bool radix_tree_get(const struct radix_tree *tree, const char *key, uintptr_t *val);
void destroy_radix_tree(struct radix_tree *tree);

#endif /* _HYVEMIND_LIB_RADIX_TREE_H */

