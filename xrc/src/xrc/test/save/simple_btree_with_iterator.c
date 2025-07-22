// #include <stdlib.h>
// #include <stdint.h>
// #include <stdbool.h>
//
// // Key/value types
// typedef uint64_t xrc_btree_key_t;
// typedef uint64_t xrc_btree_value_t;
//
// // B-tree minimum degree (t ≥ 2)
// #define XRC_BTREE_T 2
// #define XRC_BTREE_MAX_KEYS     (2*XRC_BTREE_T - 1)
// #define XRC_BTREE_MIN_KEYS     (XRC_BTREE_T - 1)
// #define XRC_BTREE_MAX_CHILDREN (2*XRC_BTREE_T)
//
// // Node: internal nodes have children; leaves have values and forward sibling link
// typedef struct xrc_btree_node {
//     bool    is_leaf;
//     int     nkeys;
//     xrc_btree_key_t   keys[XRC_BTREE_MAX_KEYS];
//     union {
//         struct xrc_btree_node *children[XRC_BTREE_MAX_CHILDREN];
//         xrc_btree_value_t      values[XRC_BTREE_MAX_KEYS];
//     };
//     struct xrc_btree_node *next_leaf;
// } xrc_btree_node;
//
// // Iterator: holds root pointer and current leaf position
// typedef struct {
//     xrc_btree_node **rootp;
//     xrc_btree_node  *leaf;
//     int              idx;
// } xrc_btree_iterator;
//
// static xrc_btree_node* xrc_btree_node_create(bool is_leaf) {
//     xrc_btree_node *n = malloc(sizeof *n);
//     n->is_leaf   = is_leaf;
//     n->nkeys     = 0;
//     n->next_leaf = NULL;
//     if (!is_leaf)
//         for (int i = 0; i < XRC_BTREE_MAX_CHILDREN; i++)
//             n->children[i] = NULL;
//     return n;
// }
//
// // Initialize tree: root is a leaf
// xrc_btree_node* xrc_btree_init(void) {
//     return xrc_btree_node_create(true);
// }
//
// // Split full child at parent->children[i]
// static void xrc_btree_split_child(xrc_btree_node *parent, int i) {
//     xrc_btree_node *full = parent->children[i];
//     xrc_btree_node *half = xrc_btree_node_create(full->is_leaf);
//     half->nkeys = XRC_BTREE_MIN_KEYS;
//     for (int j = 0; j < XRC_BTREE_MIN_KEYS; j++)
//         half->keys[j] = full->keys[j + XRC_BTREE_T];
//     if (full->is_leaf) {
//         for (int j = 0; j < XRC_BTREE_MIN_KEYS; j++)
//             half->values[j] = full->values[j + XRC_BTREE_T];
//         half->next_leaf   = full->next_leaf;
//         full->next_leaf   = half;
//     } else {
//         for (int j = 0; j < XRC_BTREE_T; j++)
//             half->children[j] = full->children[j + XRC_BTREE_T];
//     }
//     full->nkeys = XRC_BTREE_MIN_KEYS;
//     for (int j = parent->nkeys; j > i; j--)
//         parent->children[j+1] = parent->children[j];
//     parent->children[i+1] = half;
//     for (int j = parent->nkeys-1; j >= i; j--)
//         parent->keys[j+1] = parent->keys[j];
//     parent->keys[i] = full->keys[XRC_BTREE_MIN_KEYS];
//     parent->nkeys++;
// }
//
// // Traverse to leaf: split if requested, prune empty internals on the way, return iterator
// static xrc_btree_iterator xrc_btree_traverse(xrc_btree_node **rootp, xrc_btree_key_t k, bool split) {
//     xrc_btree_node *root = *rootp;
//     if (split && root->nkeys == XRC_BTREE_MAX_KEYS) {
//         xrc_btree_node *nr = xrc_btree_node_create(false);
//         nr->children[0] = root;
//         xrc_btree_split_child(nr, 0);
//         *rootp = root = nr;
//     }
//     xrc_btree_node *n = root;
//     while (!n->is_leaf) {
//         int i = 0;
//         while (i < n->nkeys && k > n->keys[i]) i++;
//         // prune empty internal child
//         while (n->children[i] && n->children[i]->nkeys == 0 && !n->children[i]->is_leaf) {
//             xrc_btree_node *empty = n->children[i];
//             n->children[i] = empty->children[0];
//             free(empty);
//         }
//         if (split && n->children[i]->nkeys == XRC_BTREE_MAX_KEYS) {
//             xrc_btree_split_child(n, i);
//             if (k > n->keys[i]) i++;
//         }
//         n = n->children[i];
//     }
//     int idx = 0;
//     while (idx < n->nkeys && k > n->keys[idx]) idx++;
//     return (xrc_btree_iterator){rootp, n, idx};
// }
//
// // Get: locate key via traverse
// bool xrc_btree_get(xrc_btree_node *root, xrc_btree_key_t k, xrc_btree_value_t *out) {
//     xrc_btree_node *r = root;
//     xrc_btree_iterator it = xrc_btree_traverse(&r, k, false);
//     xrc_btree_node *n = it.leaf;
//     int idx = it.idx;
//     if (idx < n->nkeys && n->keys[idx] == k) {
//         *out = n->values[idx];
//         return true;
//     }
//     return false;
// }
//
// // Upsert: traverse with splitting, insert or update in leaf
// bool xrc_btree_upsert(xrc_btree_node **rootp, xrc_btree_key_t k, xrc_btree_value_t v) {
//     xrc_btree_iterator it = xrc_btree_traverse(rootp, k, true);
//     xrc_btree_node *n = it.leaf;
//     int idx = it.idx;
//     if (idx < n->nkeys && n->keys[idx] == k) {
//         n->values[idx] = v;
//         return false;
//     }
//     for (int j = n->nkeys; j > idx; j--) {
//         n->keys[j]   = n->keys[j-1];
//         n->values[j] = n->values[j-1];
//     }
//     n->keys[idx]   = k;
//     n->values[idx] = v;
//     n->nkeys++;
//     return true;
// }
//
// // Remove: traverse without split, delete from leaf (lazy deletion)
// bool xrc_btree_remove(xrc_btree_node **rootp, xrc_btree_key_t k) {
//     xrc_btree_iterator it = xrc_btree_traverse(rootp, k, false);
//     xrc_btree_node *n = it.leaf;
//     int idx = it.idx;
//     if (idx >= n->nkeys || n->keys[idx] != k) return false;
//     for (int j = idx+1; j <= n->nkeys; j++) {
//         n->keys[j-1]   = n->keys[j];
//         n->values[j-1] = n->values[j];
//     }
//     n->nkeys--;
//     return true;
// }
//
// // Make iterator: wrapper around traverse
// xrc_btree_iterator xrc_btree_make_iterator(xrc_btree_node **rootp, xrc_btree_key_t k) {
//     return xrc_btree_traverse(rootp, k, false);
// }
//
// // Advance iterator forward, pruning empty leaves lazily
// bool xrc_btree_next(xrc_btree_iterator *it, xrc_btree_key_t *out_k, xrc_btree_value_t *out_v) {
//     xrc_btree_node *n = it->leaf;
//     if (!n) return false;
//     int i = it->idx;
//     if (i < n->nkeys) {
//         *out_k  = n->keys[i];
//         *out_v  = n->values[i];
//         it->idx = i + 1;
//         return true;
//     }
//     xrc_btree_node *prev = n;
//     n = n->next_leaf;
//     // skip and free empty leaves
//     while (n && n->nkeys == 0) {
//         prev->next_leaf = n->next_leaf;
//         free(n);
//         n = prev->next_leaf;
//     }
//     if (!n) return false;
//     it->leaf = n;
//     it->idx  = 1;
//     *out_k   = n->keys[0];
//     *out_v   = n->values[0];
//     return true;
// }
