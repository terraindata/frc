// #include <stdlib.h>
// #include <stdint.h>
// #include <stdbool.h>
//
// // Key/value types
// typedef uint64_t key_t;
// typedef uint64_t value_t;
//
// // B-tree minimum degree (t ≥ 2)
// #define T 2
// #define MAX_KEYS     (2*T - 1)
// #define MIN_KEYS     (T - 1)
// #define MAX_CHILDREN (2*T)
//
// // Node: internal nodes have children; leaves have values
// typedef struct Node {
//     bool    is_leaf;
//     int     nkeys;
//     key_t   keys[MAX_KEYS];
//     union {
//         struct Node *children[MAX_CHILDREN];
//         value_t      values[MAX_KEYS];
//     };
// } Node;
//
// static Node* node_create(bool is_leaf) {
//     Node *n = malloc(sizeof *n);
//     n->is_leaf = is_leaf;
//     n->nkeys    = 0;
//     if (!is_leaf)
//         for (int i = 0; i < MAX_CHILDREN; i++)
//             n->children[i] = NULL;
//     return n;
// }
//
// Node* btree_init(void) {
//     return node_create(true);
// }
//
// bool get(Node *n, key_t k, value_t *out) {
//     int i = 0;
//     while (i < n->nkeys && k > n->keys[i]) i++;
//     if (n->is_leaf)
//         return (i < n->nkeys && k == n->keys[i])
//             ? (*out = n->values[i], true)
//             : false;
//     return get(n->children[i], k, out);
// }
//
// static void split_child(Node *p, int i) {
//     Node *f = p->children[i];
//     Node *h = node_create(f->is_leaf);
//     h->nkeys = MIN_KEYS;
//     for (int j = 0; j < MIN_KEYS; j++)
//         h->keys[j] = f->keys[j + T];
//     if (f->is_leaf)
//         for (int j = 0; j < MIN_KEYS; j++)
//             h->values[j] = f->values[j + T];
//     else
//         for (int j = 0; j < T; j++)
//             h->children[j] = f->children[j + T];
//     f->nkeys = MIN_KEYS;
//     for (int j = p->nkeys; j > i; j--)
//         p->children[j + 1] = p->children[j];
//     p->children[i + 1] = h;
//     for (int j = p->nkeys - 1; j >= i; j--)
//         p->keys[j + 1] = p->keys[j];
//     p->keys[i] = f->keys[MIN_KEYS];
//     p->nkeys++;
// }
//
// static bool insert_nf(Node *n, key_t k, value_t v) {
//     int i = n->nkeys - 1;
//     if (n->is_leaf) {
//         while (i >= 0 && k < n->keys[i]) {
//             n->keys[i + 1]   = n->keys[i];
//             n->values[i + 1] = n->values[i];
//             i--;
//         }
//         if (i >= 0 && k == n->keys[i]) { n->values[i] = v; return false; }
//         n->keys[i + 1] = k;
//         n->values[i + 1] = v;
//         n->nkeys++;
//         return true;
//     }
//     while (i >= 0 && k < n->keys[i]) i--;
//     i++;
//     if (n->children[i]->nkeys == MAX_KEYS) {
//         split_child(n, i);
//         if (k > n->keys[i]) i++;
//     }
//     return insert_nf(n->children[i], k, v);
// }
//
// bool upsert(Node **rp, key_t k, value_t v) {
//     Node *r = *rp;
//     if (r->nkeys == MAX_KEYS) {
//         Node *s = node_create(false);
//         s->children[0] = r;
//         split_child(s, 0);
//         *rp = s;
//         r = s;
//     }
//     return insert_nf(r, k, v);
// }
//
// static void remove_leaf(Node *n, int i) {
//     for (++i; i <= n->nkeys; i++) {
//         n->keys[i - 1]   = n->keys[i];
//         n->values[i - 1] = n->values[i];
//     }
//     n->nkeys--;
// }
//
// static bool delete_key(Node *n, key_t k) {
//     int i = 0;
//     while (i < n->nkeys && k > n->keys[i]) i++;
//     if (n->is_leaf) {
//         if (i < n->nkeys && n->keys[i] == k)
//             return remove_leaf(n, i), true;
//         return false;
//     }
//     bool res = delete_key(n->children[i], k);
//     if (res && n->children[i]->nkeys == 0) {
//         Node *c = n->children[i];
//         free(c);
//         int ki = (i < n->nkeys ? i : i - 1);
//         for (int j = ki; j < n->nkeys; j++)
//             n->keys[j] = n->keys[j + 1];
//         for (int j = i; j <= n->nkeys; j++)
//             n->children[j] = n->children[j + 1];
//         n->nkeys--;
//     }
//     return res;
// }
//
// bool remove(Node **rp, key_t k) {
//     Node *r = *rp;
//     if (!r) return false;
//     bool got = delete_key(r, k);
//     if (r->nkeys == 0 && !r->is_leaf) {
//         Node *m = r;
//         *rp = r->children[0];
//         free(m);
//     }
//     return got;
// }
