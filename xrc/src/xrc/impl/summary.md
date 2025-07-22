# xrc/src/xrc/impl/ - Core XRC Implementation

Implementation directory containing the core data structures and algorithms for the XRC reference counting system.

## Files

### atomic_stack.h
Lock-free atomic stack implementation for concurrent data structures.
- Stack operations using atomic CAS operations
- Memory management integration with XRC reference counting

### atomic_stack_test.c
Test suite for the atomic stack implementation.
- Concurrent push/pop operations
- Memory ordering verification
- Performance benchmarking

### decrement_queue.h
Queue-based batching system for reference count decrements.
- Batches decrements to reduce contention
- Integrates with hazard pointer system
- Work-stealing queue implementation

### decrement_queue_test.c
Test suite for the decrement queue system.
- Multi-threaded enqueue/dequeue testing
- Queue overflow/underflow handling
- Integration testing with reference counting

### decrement_stack.h
Stack-based batching system for reference count decrements.
- Alternative to queue-based approach
- LIFO ordering for better cache locality

### decrement_stack_test.c
Test suite for the decrement stack implementation.
- Stack overflow/underflow scenarios
- Concurrent access patterns
- Performance comparison with queue approach

### hazard_set.c / hazard_set.h
Hazard pointer implementation for safe memory reclamation.
- Thread-local hazard pointer management
- Grace period detection for safe memory reuse
- Integration with XRC smart pointers

### tree_router.c / tree_router.h
Tree-based routing system for distributed reference counting.
- Hierarchical organization of reference count operations
- Load balancing across multiple threads
- Reduces contention on shared counters

### tree_router_test.c
Test suite for the tree router implementation.
- Multi-level tree construction and traversal
- Load distribution verification
- Performance under high contention

### possible/
Experimental implementations and research prototypes.

#### sequence_mutex.c / sequence_mutex.h
Experimental mutex implementation using sequence numbers.
- Lock-free synchronization primitive
- Ordering guarantees for concurrent operations

#### sequence_mutex_test.c
Test suite for sequence mutex.

#### skiplist.c / skiplist.h
Skip list data structure implementation.
- Probabilistic search structure
- Lock-free or low-contention variants

#### skiplist_hazard_set.h
Integration between skip list and hazard pointer system.
- Safe memory reclamation for skip list nodes
- Hazard-protected traversal operations

## Recent Changes
- Implemented core hazard pointer system
- Added batching mechanisms for decrement operations
- Experimental work on lock-free synchronization primitives
- Tree router implementation for scalable reference counting