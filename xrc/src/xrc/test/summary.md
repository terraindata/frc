# xrc/src/xrc/test/ - BTree and BST Test Implementations

Test directory containing B-tree and Binary Search Tree implementations for the XRC reference counting system.

## Files

### btree.h
B-tree header defining core data structures and interfaces for the B-tree implementation.
- `xrc_btree_header`: Node header with mutex, length, is_leaf flag, and validity
- `xrc_btree_node`: B-tree node structure with leaf/internal union
- `xrc_btree`: Main B-tree structure containing root pointer
- Key functions: `xrc_btree_initialize()`, `xrc_btree_insert()`, `xrc_btree_find()`, `xrc_btree_remove()`, `xrc_btree_dispose()`

### btree.c
B-tree implementation using raw pointers with simplified insert and find operations.
- `xrc_btree_node_initialize()`: Creates new B-tree nodes
- `xrc_btree_find()`: Binary search-based lookup
- `xrc_btree_insert()`: Insert with node splitting support
- `xrc_btree_print()`: Tree visualization helper

### btree_read_test.c
Benchmark program testing concurrent B-tree read performance with C17 threads.
- Builds random B-tree with 1M nodes
- Measures average lookup time in nanoseconds
- Uses custom barrier implementation for thread synchronization

### bst.h
Binary Search Tree header with pthread-based locking and xrc smart pointers.
- `xrc_bst_node`: BST node with mutex, key/value, and left/right pointers
- `xrc_bst`: BST structure with dummy root parent
- Inline find functions with various optimization levels

### bst.c
BST implementation with insert, remove, and find operations using xrc_sptr/xrc_lptr.
- Thread-safe operations with retry logic
- swapMin helper for node removal with two children

### bst_read_test.c
Benchmark for BST read performance, parallel to btree_read_test.c.

### raw_bst.h / raw_bst.c
Raw pointer BST implementation for performance comparison.

### raw_bst_read_test.c
Benchmark for raw pointer BST performance baseline.

### hazard_acquisition_benchmark_test.c
Performance test for hazard pointer acquisition overhead.

### memory_access_timing_test.c
Memory access timing characterization test.

### save/
Directory containing previous iterations of btree implementation using xrc_sptr.

## Recent Changes
- Fixed btree.c compilation errors by removing smart pointer dependencies
- Simplified insert implementation to use raw pointers
- Fixed print function to work without xrc_lptr/xrc_sptr
- Added placeholder for btree remove function
- All tests now compile and run successfully