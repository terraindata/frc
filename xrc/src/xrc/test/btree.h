//
// Created by ctripp on 4/1/25.
//

#pragma once
#include <stdatomic.h>
#include <xrc/pointers.h>
#include <xrc/util/slim_mutex.h>
#include <xrc/util/util.h>


typedef struct xrc_btree_header
{
    xrc_impl_slim_mutex mutex;
    int32_t             length;
    uint8_t             is_leaf;
    atomic_bool         is_valid;
} xrc_btree_header;

typedef uint64_t xrc_btree_key;
typedef uint64_t xrc_btree_value;
typedef
_Atomic
(xrc_btree_value) xrc_btree_value_atomic;

#define XRC_BTREE_MAX_DEPTH (63)
#define XRC_BTREE_NODE_SIZE (4096)
#define XRC_BTREE_ALIGNMENT (MAX(MAX(alignof(xrc_btree_header), alignof(xrc_btree_key)), alignof(xrc_btree_value)))
#define XRC_BTREE_HEADER_SIZE ((sizeof(xrc_btree_header) + XRC_BTREE_ALIGNMENT - 1) & ~(XRC_BTREE_ALIGNMENT - 1))
#define XRC_BTREE_LEAF_KEYS ((XRC_BTREE_NODE_SIZE - XRC_BTREE_HEADER_SIZE) / (sizeof(xrc_btree_key) + sizeof(xrc_btree_value)))
#define XRC_BTREE_INTERNAL_KEYS (((XRC_BTREE_NODE_SIZE - XRC_BTREE_HEADER_SIZE) / (sizeof(xrc_btree_key) + sizeof(xrc_sptr))))

typedef struct xrc_btree_leaf
{
    xrc_btree_key          keys[XRC_BTREE_LEAF_KEYS];
    xrc_btree_value_atomic values[XRC_BTREE_LEAF_KEYS];
} xrc_btree_leaf;

typedef struct xrc_btree_internal
{
    xrc_btree_key keys[XRC_BTREE_INTERNAL_KEYS];
    xrc_sptr      children[XRC_BTREE_INTERNAL_KEYS];
} xrc_btree_internal;

typedef struct xrc_btree_node
{
    xrc_btree_header header;

    union
    {
        xrc_btree_leaf     leaf;
        xrc_btree_internal internal;
    } contents;
} xrc_btree_node;


typedef struct xrc_btree
{
    xrc_sptr root;
} xrc_btree;


extern void xrc_btree_initialize(
    xrc_btree* target
    );

extern void xrc_btree_dispose(
    xrc_btree* target
    );

extern bool xrc_btree_insert(
    xrc_btree*      target,
    xrc_btree_key   key,
    xrc_btree_value value
    );

extern bool xrc_btree_remove(
    xrc_btree*    target,
    xrc_btree_key key
    );

extern bool xrc_btree_find(
    xrc_btree*       target,
    xrc_btree_key    key,
    xrc_btree_value* value
    );
