#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>

/**
 * We can store data nodes at indices [1..65535] in a global array.
 * Index 0 is "NULL".
 */
#define XRC_SKIP_LIST_CAPACITY 65536
#define NULL_INDEX 0

/**
 * Typical x64 cache line is 64 bytes. We'll store ~128 bytes per node
 * (two lines) so that each node has its own lock, version, and next array.
 */
#define CACHE_LINE_SIZE 64

/**
 * We pick MAX_LEVEL=8 or 16. For brevity, let's do 8.
 */
#define MAX_LEVEL 8
#define PROBABILITY 0.5

/**
 * The user data, sorted by (address, epoch).
 * If tie, we could break by pointer addresses, but omitted here for brevity.
 */
typedef struct
{
    intptr_t address;
    size_t   epoch;
} xrc_impl_guard;

/**
 * Each skiplist_node is about 128 bytes:
 *
 * - next[MAX_LEVEL]: 8 * 2 = 16 bytes (16-bit indexes).
 * - value: 16 bytes
 * - version: 8 bytes
 * - lock: ~40 bytes
 * - padding to reach 128 total.
 */
typedef struct skiplist_node
{
    /**
     * 'next[i]' is a 16-bit index into global_nodes[] (1..65535).
     * If next[i] == 0, that link is null.
     */
    uint_least16_t next[MAX_LEVEL];

    /**
     * The stored value (address, epoch).
     */
    xrc_impl_guard value;

    /**
     * Version counter incremented on lock/unlock for optimistic concurrency checks.
     */
    _Atomic
    uint64_t version;

    /**
     * Per-node lock for insertion/removal.
     */
    pthread_mutex_t lock;

    /**
     * Optional padding to bring total size up to ~128 bytes.
     */
    char pad[CACHE_LINE_SIZE * 2
             - (sizeof(uint_least16_t) * MAX_LEVEL
                + sizeof(xrc_impl_guard)
                + sizeof(
    _Atomic
    uint64_t
    )
    +
    sizeof
    (
    pthread_mutex_t
    )
    )
    ];
} skiplist_node;

/** The global array of real data nodes. index=0 => null. */
extern skiplist_node global_nodes[XRC_SKIP_LIST_CAPACITY];

/**
 * Initialize a sentinel node (allocated anywhere, e.g. on the heap).
 * The sentinel has minimal key (INTPTR_MIN, epoch=0), next[] = 0, version=0, etc.
 */
void skiplist_init(
    skiplist_node* sentinel
    );

/**
 * Insert a new node from global_nodes[] into the skip list.
 * - 'sentinel' is the pointer to the skip list's sentinel node.
 * - 'newNode' is a pointer to a node in global_nodes[] (with its own lock, etc.).
 */
bool skiplist_insert(
    skiplist_node* sentinel,
    skiplist_node* newNode
    );

/**
 * Remove 'targetNode' (in global_nodes) from the skip list whose sentinel is 'sentinel'.
 * The caller guarantees 'targetNode' is present in the list.
 * Returns the same pointer after unlinking.
 */
skiplist_node* skiplist_remove(
    skiplist_node* sentinel,
    skiplist_node* targetNode
    );

/**
 * Query the skip list (given by 'sentinel') for the smallest element >= 'key'.
 * Returns true if found, storing the element in *outValue; else false.
 *
 * Uses an optimistic read from level=MAX_LEVEL-1 down to 0.
 */
bool skiplist_query(
    skiplist_node*  sentinel,
    xrc_impl_guard  key,
    xrc_impl_guard* outValue
    );

#endif // SKIPLIST_H
