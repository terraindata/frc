# xrc/src/xrc/ - XRC Reference Counting Core

Core directory for the XRC (eXtended Reference Counter) implementation - a new semi-deferred reference counting system designed for high-performance concurrent applications in C and C++.

## Overview

XRC is a next-generation reference counting system that builds upon the lessons learned from FRC (Fast Reference Counter). It provides:
- Semi-deferred reference counting for reduced contention
- Hazard pointer-based memory reclamation
- Lock-free and wait-free data structures
- Thread-safe smart pointers (xrc_sptr, xrc_lptr)
- Batched decrement operations for performance

## Core Files

### pointers.h
Primary interface for XRC smart pointers.
- **xrc_sptr**: Shared pointer with atomic reference counting
- **xrc_lptr**: Local pointer protected by hazard pointers
- Comprehensive API for pointer initialization, assignment, and disposal
- Support for both atomic and non-atomic operations
- Integration with memory allocation and reference counting

### settings.h
Configuration parameters and compile-time settings.
- Thread limits and capacity settings (XRC_MAX_THREADS = 2048)
- Hazard pointer configuration (128 per thread)
- Decrement queue/stack sizing (8192 entries each)
- Performance tuning parameters for epoch advancement
- Type definitions for reference counts and allocation metadata

### thread_data.h / thread_data.c
Thread-local data management and memory allocation.
- **xrc_impl_thread_data**: Per-thread state including hazard sets and decrement queues
- **xrc_impl_control_data_s**: Global system state and epoch management
- Memory allocation with reference counting headers
- Support for custom dispose functions and extended metadata
- Thread attach/detach lifecycle management

### pointers_test.c
Comprehensive test suite for pointer operations.
- Multi-threaded stress testing
- Correctness verification for all pointer operations
- Performance benchmarking and regression testing

## Subdirectories

### impl/
Core implementation of concurrent data structures and algorithms.
- Atomic stacks and lock-free operations
- Decrement batching systems (queue and stack)
- Hazard pointer set implementation
- Tree router for distributed reference counting
- Experimental lock-free synchronization primitives

### test/
Test implementations and benchmarks.
- B-tree implementation (btree.c, btree.h) - currently working
- Binary search tree (bst.c, bst.h, raw_bst.c, raw_bst.h)
- Performance benchmarks for concurrent data structures
- Memory access timing characterization

### util/
Utility functions and cross-platform support.
- Thread affinity management (bind_thread_to_core)
- Lightweight mutex implementations (slim_mutex)
- Random number generation (SplitMix64)
- Platform-specific optimizations

## Architecture

XRC uses a multi-layered approach:
1. **Smart Pointers**: High-level interface (xrc_sptr/xrc_lptr)
2. **Hazard Pointers**: Safe memory reclamation without locks
3. **Batched Decrements**: Queue-based batching reduces contention
4. **Epoch-based Garbage Collection**: Grace period detection
5. **Tree Router**: Hierarchical organization for scalability

## Memory Management

- **Reference Counting**: Atomic counters with batched decrements
- **Hazard Protection**: Thread-local hazard pointers prevent premature deallocation
- **Custom Allocators**: Support for dispose functions and extended metadata
- **Alignment Handling**: Proper alignment for various data types

## Recent Changes
- Implemented working B-tree with raw pointers (no smart pointers)
- Fixed compilation errors in test implementations
- Enhanced hazard pointer integration
- Improved performance tuning parameters
- Added comprehensive test coverage

## Thread Safety
- Lock-free operations where possible
- Wait-free read operations using hazard pointers
- Epoch-based synchronization for garbage collection
- Careful memory ordering throughout the implementation

## Performance Features
- Batched reference count operations
- Cache-line padding to avoid false sharing
- Thread affinity support for NUMA systems
- Optimized fast paths for common operations