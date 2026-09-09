#include "fatal.h"
#include "halloc.h"
#include "printf.h"
#include "string.h"
#include "lib/radix-tree.h"

static inline void
destroy_radix_node(struct radix_node *node)
{
    for (int i = 0; i < RADIX_MAX_NR_CHILDREN; i++) {
        if (node->children[i]) {
            destroy_radix_node(node->children[i]);
        }
    }

    /* We never allocate memory for the root node prefix */
    if (node->prefix) {
        hfree(node->prefix);
    }

    hfree(node);
}

void
destroy_radix_tree(const struct radix_tree *tree)
{
    if (!tree) {
        return;
    }

    if (tree->root) {
        destroy_radix_node(tree->root);
    }

    hfree(tree);
}

static inline struct radix_node *
allocate_radix_node(void)
{
    struct radix_node *node = (struct radix_node *) hmalloc(sizeof(struct radix_node));
    if (!node) {
        pr_error("Error allocating radix node");
        return NULL;
    }

    memset(node, 0, sizeof(*node));
    return node;
}

struct radix_tree *
create_radix_tree(void)
{
    struct radix_tree *tree = (struct radix_tree *) hmalloc(sizeof(struct radix_tree));
    if (!tree) {
        pr_error("Error allocating radix tree struct");
        return NULL;
    }

    tree->root = allocate_radix_node();
    if (!tree->root) {
        pr_error("Error allocating radix tree's root node");
        hfree(tree);
        return NULL;
    }

    return tree;
}

static inline uint8_t
get_key_index(const char *key)
{
    return (uint8_t) key[0];
}

static int
get_common_prefix_length(const char *child_prefix, const char *key)
{
    const int key_len = strlen(key);
    const int child_prefix_len = strlen(child_prefix);

    int common_prefix_pos = 0;
    while ((common_prefix_pos < key_len) && (common_prefix_pos < child_prefix_len)) {
        if (child_prefix[common_prefix_pos] != key[common_prefix_pos]) {
            break;
        }
        common_prefix_pos++;
    }

    return common_prefix_pos;
}

static bool
__radix_tree_get(const struct radix_node *node, const char *key, uintptr_t *val)
{
    if (key[0] == '\0') {
        if (node->terminating) {
            *val = node->data;
            return true;
        }
        return false;
    }

    const uint8_t index = get_key_index(key);
    const struct radix_node *child = node->children[index];

    if (!child) {
        return false;
    }

    const int child_prefix_len = strlen(child->prefix);
    const int common_prefix_len = get_common_prefix_length(child->prefix, key);

    if (common_prefix_len < child_prefix_len) {
        return false;
    }

    return __radix_tree_get(child, &key[common_prefix_len], val);
}

bool
radix_tree_get(const struct radix_tree *tree, const char *key, uintptr_t *val)
{
    return __radix_tree_get(tree->root, key, val);
}

bool
radix_tree_contains(const struct radix_tree *tree, const char *key)
{
    uintptr_t dummy;
    return radix_tree_get(tree, key, &dummy);
}

static bool
__radix_tree_add(struct radix_node *node, const char *key, const uintptr_t data)
{
    if (key[0] == '\0') {
        node->terminating = true;
        node->data = data;
        return true;
    }

    const uint8_t index = get_key_index(key);
    struct radix_node *child = node->children[index];

    if (child == NULL) {
        child = allocate_radix_node();
        if (!child) {
            pr_error("Error allocating radix node for prefix %s", key);
            return false;
        }

        child->prefix = strdup_nt(key);
        node->children[index] = child;
        return __radix_tree_add(child, &key[strlen(key)], data);
    }

    const int child_prefix_len = strlen(child->prefix);
    const int common_prefix_len = get_common_prefix_length(child->prefix, key);

    if (common_prefix_len == child_prefix_len) {
        return __radix_tree_add(child, &key[common_prefix_len], data);
    }

    struct radix_node *insert_node = allocate_radix_node();
    if (!insert_node) {
        pr_error("Error creating node inserted to split prefix '%s' for key '%s'",
                child->prefix,
                key
        );
        return false;
    }

    insert_node->prefix = strndup_nt(child->prefix, common_prefix_len);
    node->children[index] = insert_node;

    char *old_prefix = child->prefix;

    char *split_subtree_prefix = strdup_nt(&old_prefix[common_prefix_len]);
    child->prefix = split_subtree_prefix;
    insert_node->children[get_key_index(split_subtree_prefix)] = child;

    hfree(old_prefix);

    return __radix_tree_add(insert_node, &key[common_prefix_len], data);
}

bool
radix_tree_add(struct radix_tree *tree, const char *key, const uintptr_t data)
{
    return __radix_tree_add(tree->root, key, data);
}

