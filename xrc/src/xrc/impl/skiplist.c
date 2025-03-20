#include "skiplist.h"
#include <assert.h>

/** The global array of data nodes. */
skiplist_node global_nodes[XRC_SKIP_LIST_CAPACITY];

/** Convert index => pointer, or NULL if 0. */
static inline skiplist_node* node_ptr(
    uint_least16_t idx
    )
{
    return (idx == 0) ? NULL : &global_nodes[idx];
}

/** Convert pointer => index.
 *  The caller must ensure 'p' is within the global_nodes[] array if p!=sentinel.
 */
static inline uint_least16_t node_index(
    const skiplist_node* p
    )
{
    if (!p)
        return 0;
    // If 'p' is the sentinel, we do not store it in the global array => no valid index.
    // But we only call node_index on data nodes in this code.
    return (uint_least16_t)(p - global_nodes);
}

/** Compare xrc_impl_guard by (address, epoch). Return <0,0,>0. */
static int compare_xrc(
    const xrc_impl_guard* a,
    const xrc_impl_guard* b
    )
{
    if (a->address < b->address)
        return -1;
    if (a->address > b->address)
        return 1;
    if (a->epoch < b->epoch)
        return -1;
    if (a->epoch > b->epoch)
        return 1;
    return 0;
}

/** random_level: produce 1..MAX_LEVEL. */
static int random_level(
    void
    )
{
    int level = 1;
    while ((rand() & 0xFFFF) < (int)(PROBABILITY * 0xFFFF) && level < MAX_LEVEL)
    {
        level++;
    }
    return level;
}

/** lock_node(node):
 *  Acquire node->lock, increment node->version => "start update."
 */
static inline void lock_node(
    skiplist_node* node
    )
{
    pthread_mutex_lock(&node->lock);
    uint64_t ver = atomic_load_explicit(&node->version, /*mo=*/memory_order_relaxed);
    atomic_store_explicit(&node->version, ver + 1, memory_order_release);
}

/** unlock_node(node):
 *  increment version => "end update," then unlock.
 */
static inline void unlock_node(
    skiplist_node* node
    )
{
    uint64_t ver = atomic_load_explicit(&node->version, memory_order_relaxed);
    atomic_store_explicit(&node->version, ver + 1, memory_order_release);
    pthread_mutex_unlock(&node->lock);
}

/** version_read(node): for optimistic concurrency checks. */
static inline uint64_t version_read(
    const skiplist_node* node
    )
{
    return atomic_load_explicit(&node->version, memory_order_acquire);
}

/** skiplist_init(sentinel):
 *  Initialize a sentinel node with minimal key, next[i]=0, version=0, etc.
 */
void skiplist_init(
    skiplist_node* sentinel
    )
{
    // minimal key => effectively -∞
    sentinel->value.address = INTPTR_MIN;
    sentinel->value.epoch   = 0;

    for (int i = 0; i < MAX_LEVEL; i++)
    {
        sentinel->next[i] = 0; // no link
    }
    atomic_store_explicit(&sentinel->version, 0, memory_order_relaxed);
    pthread_mutex_init(&sentinel->lock, NULL);

    srand((unsigned)time(NULL));
}

/** skiplist_insert(sentinel, newNode):
 *  1) pick random level
 *  2) top-down search using optimistic checks
 *  3) lock predecessor nodes for levels < newLevel
 *  4) link newNode, unlock
 */
bool skiplist_insert(
    skiplist_node* sentinel,
    skiplist_node* newNode
    )
{
    int lvl = random_level();

    // We'll store predecessor pointers (which might be the sentinel or real nodes).
    skiplist_node* update[MAX_LEVEL];

    // top-down search
    skiplist_node* curr = sentinel;
    for (int level = MAX_LEVEL - 1; level >= 0; level--)
    {
        while (true)
        {
            // read version => check if changed
            uint64_t       startVer = version_read(curr);
            uint_least16_t nxIdx    = curr->next[level];
            uint64_t       endVer   = version_read(curr);
            if (startVer != endVer)
            {
                return false; // concurrency => caller can retry
            }

            if (nxIdx == 0)
            {
                break; // no link => go down
            }
            skiplist_node* nx = node_ptr(nxIdx);

            // check nx version
            uint64_t nxVer1 = version_read(nx);
            uint64_t nxVer2 = version_read(nx);
            if (nxVer1 != nxVer2)
            {
                return false; // concurrency => retry
            }

            int cmp = compare_xrc(&nx->value, &newNode->value);
            if (cmp < 0)
            {
                curr = nx;
            }
            else
            {
                break;
            }
        }
        update[level] = curr;
    }

    // lock the needed predecessors in ascending order
    for (int i = 0; i < lvl; i++)
    {
        lock_node(update[i]);
    }

    // validate none changed in a conflicting way
    bool valid = true;
    for (int i = 0; i < lvl; i++)
    {
        uint_least16_t succIdx = update[i]->next[i];
        if (succIdx != 0)
        {
            skiplist_node* succ = node_ptr(succIdx);
            if (compare_xrc(&succ->value, &newNode->value) < 0)
            {
                valid = false;
                break;
            }
        }
    }

    if (!valid)
    {
        // unlock in reverse
        for (int i = lvl - 1; i >= 0; i--)
        {
            unlock_node(update[i]);
        }
        return false;
    }

    // link newNode
    pthread_mutex_init(&newNode->lock, NULL);
    atomic_store_explicit(&newNode->version, 0, memory_order_relaxed);

    for (int i = 0; i < lvl; i++)
    {
        uint_least16_t oldNext = update[i]->next[i];
        newNode->next[i]       = oldNext;
        update[i]->next[i]     = (uint_least16_t)(newNode - global_nodes);
    }

    // unlock in reverse
    for (int i = lvl - 1; i >= 0; i--)
    {
        unlock_node(update[i]);
    }

    return true;
}

/** skiplist_remove(sentinel, targetNode):
 *  - The caller guarantees targetNode is in the list.
 *  - find preds from top..0, lock them + lock target if pred->next[i] == target
 *  - unlink
 */
skiplist_node* skiplist_remove(
    skiplist_node* sentinel,
    skiplist_node* targetNode
    )
{
    skiplist_node* update[MAX_LEVEL];
    skiplist_node* curr = sentinel;
    xrc_impl_guard tVal = targetNode->value;

    // 1) gather preds
    for (int level = MAX_LEVEL - 1; level >= 0; level--)
    {
        while (true)
        {
            uint64_t       startVer = version_read(curr);
            uint_least16_t nxIdx    = curr->next[level];
            uint64_t       endVer   = version_read(curr);
            if (startVer != endVer)
            {
                return NULL; // concurrency => caller can retry
            }
            if (nxIdx == 0)
            {
                break;
            }
            skiplist_node* nx     = node_ptr(nxIdx);
            uint64_t       nxVer1 = version_read(nx);
            uint64_t       nxVer2 = version_read(nx);
            if (nxVer1 != nxVer2)
            {
                return NULL;
            }

            int cmp = compare_xrc(&nx->value, &tVal);
            if (cmp < 0)
            {
                curr = nx;
            }
            else
            {
                break;
            }
        }
        update[level] = curr;
    }

    // 2) lock them for levels that actually reference targetNode
    bool lockedLevel[MAX_LEVEL];
    for (int i = 0; i < MAX_LEVEL; i++)
    {
        lockedLevel[i] = false;
    }
    int            topLevel = 0;
    uint_least16_t targIdx  = (uint_least16_t)(targetNode - global_nodes);

    for (int i = 0; i < MAX_LEVEL; i++)
    {
        if (update[i]->next[i] == targIdx)
        {
            lock_node(update[i]);
            lockedLevel[i] = true;
            if (i + 1 > topLevel)
            {
                topLevel = i + 1;
            }
        }
    }

    // lock the target too
    lock_node(targetNode);

    // 3) validate and unlink
    bool valid = true;
    for (int i = 0; i < topLevel; i++)
    {
        if (lockedLevel[i])
        {
            if (update[i]->next[i] != targIdx)
            {
                valid = false;
                break;
            }
        }
    }

    if (valid)
    {
        // do the unlink
        for (int i = 0; i < topLevel; i++)
        {
            if (lockedLevel[i])
            {
                update[i]->next[i] = targetNode->next[i];
            }
        }
    }

    // 4) unlock in reverse
    unlock_node(targetNode);
    for (int i = topLevel - 1; i >= 0; i--)
    {
        if (lockedLevel[i])
        {
            unlock_node(update[i]);
        }
    }

    return valid ? targetNode : NULL;
}

/** skiplist_query(sentinel, key, outValue):
 *  an optimistic read from top=MAX_LEVEL-1..0
 */
bool skiplist_query(
    skiplist_node*  sentinel,
    xrc_impl_guard  key,
    xrc_impl_guard* outValue
    )
{
retry_query:
    {
        skiplist_node* curr = sentinel;
        for (int level = MAX_LEVEL - 1; level >= 0; level--)
        {
            while (true)
            {
                uint64_t       startVer = version_read(curr);
                uint_least16_t nxIdx    = curr->next[level];
                uint64_t       endVer   = version_read(curr);
                if (startVer != endVer)
                {
                    goto retry_query;
                }
                if (nxIdx == 0)
                {
                    break;
                }
                skiplist_node* nx = node_ptr(nxIdx);

                uint64_t nxVer1 = version_read(nx);
                uint64_t nxVer2 = version_read(nx);
                if (nxVer1 != nxVer2)
                {
                    goto retry_query;
                }

                int cmp = compare_xrc(&nx->value, &key);
                if (cmp < 0)
                {
                    curr = nx;
                }
                else
                {
                    break;
                }
            }
        }

        // final candidate at level 0
        uint64_t       sVer1   = version_read(curr);
        uint_least16_t candIdx = curr->next[0];
        uint64_t       sVer2   = version_read(curr);
        if (sVer1 != sVer2)
        {
            goto retry_query;
        }
        if (candIdx == 0)
        {
            return false; // none >= key
        }
        skiplist_node* cand = node_ptr(candIdx);

        uint64_t cVer1 = version_read(cand);
        int      cmp   = compare_xrc(&cand->value, &key);
        uint64_t cVer2 = version_read(cand);
        if (cVer1 != cVer2)
        {
            goto retry_query;
        }
        if (cmp >= 0)
        {
            *outValue = cand->value;
            // final check
            uint64_t cVer3 = version_read(cand);
            if (cVer3 != cVer2)
            {
                goto retry_query;
            }
            return true;
        }
        return false;
    }
}
