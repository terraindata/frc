# xrc/src/xrc/util/ - XRC Utility Functions

Utility directory containing helper functions, synchronization primitives, and system-level utilities for the XRC reference counting system.

## Files

### bind_thread_to_core.c / bind_thread_to_core.h
Thread affinity management for performance optimization.
- Binds threads to specific CPU cores
- Reduces cache misses and context switching overhead
- Platform-specific implementations (Linux, Windows)
- Used in benchmarks and high-performance scenarios

### mutex_util.c / mutex_util.h
Mutex utility functions and wrappers.
- Cross-platform mutex abstractions
- Performance monitoring and debugging support
- Integration with XRC's locking strategies

### slim_mutex.c / slim_mutex.h
Lightweight mutex implementation optimized for XRC use cases.
- Reduced memory footprint compared to standard mutexes
- Fast-path optimizations for uncontended locks
- Integration with hazard pointer system
- Used throughout XRC data structures

### sim_mutex_test.c
Test suite for slim mutex implementation.
- Correctness verification under contention
- Performance benchmarking against standard mutexes
- Stress testing with multiple threads

### splitmix.h
SplitMix64 pseudo-random number generator.
- Fast, high-quality random number generation
- Used for hash functions and probabilistic algorithms
- Based on the SplitMix64 algorithm
- Header-only implementation

### util.h
General utility macros and helper functions.
- Common mathematical operations (MAX, MIN, etc.)
- Bit manipulation utilities
- Alignment and padding helpers
- Platform-specific definitions

## Purpose
These utilities provide the foundational support for:
- High-performance concurrent data structures
- Cross-platform compatibility
- Performance optimization through thread affinity
- Lightweight synchronization primitives
- Quality random number generation for algorithms

## Recent Changes
- Enhanced thread binding utilities for multi-core systems
- Optimized slim mutex for XRC workloads
- Added comprehensive test coverage for all utilities
- Platform-specific optimizations for Linux systems