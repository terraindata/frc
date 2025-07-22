// /* =====================================================================
//  *  4KiB‑node B‑tree – minimal layout (no header struct, no unions)
//  * ===================================================================== */
// #include <stdlib.h>
// #include <stdint.h>
// #include <stdbool.h>
// #include <stddef.h>
// #include <string.h>
// #include <assert.h>
// #include <stdalign.h>
//
// /* ---- public types & basic params ----------------------------------- */
// typedef uint64_t xrc_btree_key_t;
// typedef uint64_t xrc_btree_value_t;
// #define XRC_BTREE_NODE_SIZE  (4096u - 2u * sizeof(size_t))
//
// /* -------------------------------------------------------------------- */
// /*  node definition – header fields first, then a flexible key array    */
// /* -------------------------------------------------------------------- */
// typedef struct xrc_btree_node
// {
//     struct xrc_btree_node* next; /* horizontal sibling             */
//     int32_t                length; /* #elements in this node         */
//     bool                   is_leaf;
//     xrc_btree_key_t        keys[]; /* starts after padded header     */
// } xrc_btree_node;
//
// typedef struct
// {
//     xrc_btree_node** rootp;
//     xrc_btree_node*  leaf;
//     int32_t          idx;
// } xrc_btree_iter;
//
// /* -------------------------------------------------------------------- */
// /*  compile‑time layout & capacities                                    */
// /* -------------------------------------------------------------------- */
//
// /* ---- alignment  helper */
// #define XRC_BTREE_ALIGN_UP(V,A)  (((V) + (A) - 1u) / (A) * (A))
//
// enum
// {
//     /* padded start of keys[]                                          */
//     XRC_BTREE_KEYS_OFFSET =
//     XRC_BTREE_ALIGN_UP(
//         sizeof(xrc_btree_node) - sizeof(xrc_btree_key_t*),
//         alignof(xrc_btree_key_t)
//         ),
//
//     /* element sizes                                                   */
//     XRC_BTREE_LEAF_PAIR_SIZE =
//     sizeof(xrc_btree_key_t) + sizeof(xrc_btree_value_t),
//     XRC_BTREE_INT_PAIR_SIZE =
//     sizeof(xrc_btree_key_t) + sizeof(xrc_btree_node*),
//
//     /* capacities (throw away one‑element worst‑case padding)          */
//     XRC_BTREE_LEAF_CAPACITY =
//     (int)((XRC_BTREE_NODE_SIZE - XRC_BTREE_KEYS_OFFSET
//            - alignof(xrc_btree_value_t)) / XRC_BTREE_LEAF_PAIR_SIZE),
//
//     XRC_BTREE_INTERNAL_CAPACITY =
//     (int)((XRC_BTREE_NODE_SIZE - XRC_BTREE_KEYS_OFFSET
//            - alignof(xrc_btree_node*)) / XRC_BTREE_INT_PAIR_SIZE),
//
//     /* offsets of secondary arrays                                     */
//     XRC_BTREE_LEAF_VALS_OFFSET =
//     XRC_BTREE_ALIGN_UP(
//         XRC_BTREE_KEYS_OFFSET +
//         XRC_BTREE_LEAF_CAPACITY * sizeof(xrc_btree_key_t),
//         alignof(xrc_btree_value_t)
//         ),
//
//     XRC_BTREE_INT_CHILD_OFFSET =
//     XRC_BTREE_ALIGN_UP(
//         XRC_BTREE_KEYS_OFFSET +
//         XRC_BTREE_INTERNAL_CAPACITY * sizeof(xrc_btree_key_t),
//         alignof(xrc_btree_node*)
//         )
// };
//
// /* compile‑time safety                                                 */
// _Static_assert(XRC_BTREE_LEAF_CAPACITY >= 2, "node too small");
// _Static_assert(XRC_BTREE_INTERNAL_CAPACITY >= 2, "node too small");
//
// /* -------------------------------------------------------------------- */
// /*  tiny helpers for the *complicated* parts (pointer arithmetic)       */
// /* -------------------------------------------------------------------- */
// static inline xrc_btree_value_t* xrc_btree_values(
//     xrc_btree_node* n
//     )
// {
//     return (xrc_btree_value_t*)((char*)n + XRC_BTREE_LEAF_VALS_OFFSET);
// }
//
// static inline xrc_btree_node** xrc_btree_children(
//     xrc_btree_node* n
//     )
// {
//     return (xrc_btree_node**)((char*)n + XRC_BTREE_INT_CHILD_OFFSET);
// }
//
// /* -------------------------------------------------------------------- */
// /*  allocation                                                          */
// /* -------------------------------------------------------------------- */
// static xrc_btree_node* xrc_btree_node_create(
//     int32_t length,
//     bool    leaf
//     )
// {
//     xrc_btree_node* n = aligned_alloc(alignof(max_align_t), XRC_BTREE_NODE_SIZE);
//     n->next           = NULL;
//     n->length         = length;
//     n->is_leaf        = leaf;
//     if (!leaf)
//     {
//         for (int32_t i               = 0; i < XRC_BTREE_INTERNAL_CAPACITY; ++i)
//             xrc_btree_children(n)[i] = NULL;
//     }
//     return n;
// }
//
// /* -------------------------------------------------------------------- */
// /*  universal range move                                                */
// /* -------------------------------------------------------------------- */
// static void xrc_btree_move_range(
//     xrc_btree_node* src,
//     int32_t         s,
//     xrc_btree_node* dst,
//     int32_t         d,
//     int32_t         n
//     )
// {
//     if (!n)
//         return;
//
//     memmove(dst->keys + d, src->keys + s, n * sizeof(xrc_btree_key_t));
//
//     if (src->is_leaf)
//     {
//         memmove(
//             xrc_btree_values(dst) + d,
//             xrc_btree_values(src) + s,
//             n * sizeof(xrc_btree_value_t)
//             );
//     }
//     else
//     {
//         memmove(
//             xrc_btree_children(dst) + d,
//             xrc_btree_children(src) + s,
//             n * sizeof(xrc_btree_node*)
//             );
//     }
// }
//
// /* -------------------------------------------------------------------- */
// /*  binary predecessor search                                           */
// /* -------------------------------------------------------------------- */
// static int32_t xrc_btree_find_idx(
//     xrc_btree_node* n,
//     xrc_btree_key_t k
//     )
// {
//     int32_t lo = 0, hi = n->length;
//     while (lo < hi)
//     {
//         int32_t mid = (lo + hi) >> 1;
//         if (n->keys[mid] <= k)
//             lo = mid + 1;
//         else
//             hi = mid;
//     }
//     return lo - 1;
// }
//
// /* -------------------------------------------------------------------- */
// /*  split child                                                         */
// /* -------------------------------------------------------------------- */
// static void xrc_btree_split_child(
//     xrc_btree_node* p,
//     int32_t         i
//     )
// {
//     xrc_btree_node** ch          = xrc_btree_children(p);
//     xrc_btree_node*  full        = ch[i];
//     int32_t          left_length = full->length / 2;
//     xrc_btree_node*  half        = xrc_btree_node_create(full->length - left_length, full->is_leaf);
//
//     xrc_btree_move_range(full, left_length, half, 0, half->length);
//     full->length = left_length;
//
//     half->next = full->next;
//     full->next = half;
//
//     xrc_btree_key_t promote = half->keys[0];
//     xrc_btree_move_range(p, i + 1, p, i + 2, p->length - (i + 1));
//     ch             = xrc_btree_children(p); /* may have shifted */
//     ch[i + 1]      = half;
//     p->keys[i + 1] = promote;
//     p->length++;
// }
//
// /* -------------------------------------------------------------------- */
// /*  xrc_btree_balance                                                             */
// /* -------------------------------------------------------------------- */
// #define XRC_BTREE_BAL_THRESHOLD  (XRC_BTREE_LEAF_CAPACITY / 2)
// #define XRC_BTREE_MERGE_THRESHOLD (2*XRC_BTREE_LEAF_CAPACITY / 3)
//
// /* returns true  ⇢  merge or redistribution occurred (layout changed)
//  *         false ⇢  nothing done, current indices still valid          */
// static bool xrc_btree_balance(
//     xrc_btree_node* p,
//     int32_t         i
//     )
// {
//     xrc_btree_node* l = xrc_btree_children(p)[i];
//     if (l->length >= XRC_BTREE_BAL_THRESHOLD || i + 1 >= p->length)
//         return false; /* no work */
//
//     xrc_btree_node* r     = xrc_btree_children(p)[i + 1];
//     int32_t         total = l->length + r->length;
//
//     if (total < XRC_BTREE_MERGE_THRESHOLD)
//     {
//         /* merge */
//         xrc_btree_move_range(r, 0, l, l->length, r->length);
//         l->length = total;
//         l->next   = r->next;
//         free(r);
//         xrc_btree_move_range(p, i + 2, p, i + 1, p->length - i - 2);
//         p->length--;
//         return true;
//     }
//
//     /* redistribute */
//     int32_t target = total >> 1;
//     if (l->length != target)
//     {
//         if (l->length < target)
//         {
//             int32_t need = target - l->length;
//             xrc_btree_move_range(r, 0, l, l->length, need);
//             xrc_btree_move_range(r, need, r, 0, r->length - need);
//             l->length += need;
//             r->length -= need;
//         }
//         else
//         {
//             int32_t give = l->length - target;
//             xrc_btree_move_range(r, 0, r, give, r->length);
//             xrc_btree_move_range(l, l->length - give, r, 0, give);
//             l->length -= give;
//             r->length += give;
//         }
//         p->keys[i + 1] = r->keys[0];
//         return true; /* changed */
//     }
//     return false; /* already balanced */
// }
//
//
// /* --------------------------------------------------------------------
//  *  traversal helper  (only the inner loop changed)
//  * ------------------------------------------------------------------ */
// static xrc_btree_iter xrc_btree_traverse(
//     xrc_btree_node** rootp,
//     xrc_btree_key_t  k,
//     bool             split
//     )
// {
//     xrc_btree_node* n;
//     for (;;)
//     {
//         n = *rootp;
//         /* grow when splitting and the root is full */
//         if (!split || n->length < XRC_BTREE_INTERNAL_CAPACITY)
//             break;
//
//         xrc_btree_node* new_root        = xrc_btree_node_create(1, false);
//         new_root->keys[0]               = n->keys[0];
//         xrc_btree_children(new_root)[0] = n;
//         *rootp                          = new_root;
//     }
//
//     while (!n->is_leaf)
//     {
//         int32_t ci            = xrc_btree_find_idx(n, k);
//         ci                    = ci >= 0 ? ci : 0;
//         xrc_btree_node* child = xrc_btree_children(n)[ci];
//
//         /* split if the chosen child is still full                         */
//         if (split && child->length >= XRC_BTREE_INTERNAL_CAPACITY)
//         {
//             xrc_btree_split_child(n, ci);
//             continue;
//         }
//
//         if (xrc_btree_balance(n, ci))
//             continue; /* merge or redistribution occurred */
//
//         n = child;
//     }
//
//     return (xrc_btree_iter){rootp, n, xrc_btree_find_idx(n, k)};
// }
//
//
// /* -------------------------------------------------------------------- */
// /*  public API                                                          */
// /* -------------------------------------------------------------------- */
// xrc_btree_node* xrc_btree_init(
//     void
//     )
// {
//     return xrc_btree_node_create(0, true);
// }
//
// bool xrc_btree_get(
//     xrc_btree_node*    root,
//     xrc_btree_key_t    k,
//     xrc_btree_value_t* out
//     )
// {
//     if (!root)
//         return false;
//     xrc_btree_iter it = xrc_btree_traverse(&root, k, false);
//     if (it.idx >= 0 && it.leaf->keys[it.idx] == k)
//     {
//         *out = xrc_btree_values(it.leaf)[it.idx];
//         return true;
//     }
//     return false;
// }
//
// bool xrc_btree_upsert(
//     xrc_btree_node**  rootp,
//     xrc_btree_key_t   k,
//     xrc_btree_value_t v
//     )
// {
//     xrc_btree_iter     it   = xrc_btree_traverse(rootp, k, true);
//     xrc_btree_node*    n    = it.leaf;
//     xrc_btree_value_t* vals = xrc_btree_values(n);
//
//     if (it.idx >= 0 && n->keys[it.idx] == k)
//     {
//         vals[it.idx] = v;
//         return false;
//     }
//
//     int32_t pos = it.idx + 1;
//     xrc_btree_move_range(n, pos, n, pos + 1, n->length - pos);
//     n->keys[pos] = k;
//     vals[pos]    = v;
//     n->length++;
//     return true;
// }
//
// bool xrc_btree_remove(
//     xrc_btree_node** rootp,
//     xrc_btree_key_t  k
//     )
// {
//     xrc_btree_iter  it = xrc_btree_traverse(rootp, k, false);
//     xrc_btree_node* n  = it.leaf;
//     if (it.idx < 0 || it.idx >= n->length || n->keys[it.idx] != k)
//         return false;
//
//     xrc_btree_move_range(n, it.idx + 1, n, it.idx, n->length - it.idx - 1);
//     n->length--;
//     return true;
// }
