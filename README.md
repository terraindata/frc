Fast Reference Counter (FRC)

(C) 2017, 2018 Terrain Data, Inc.

## Introduction

FRC is a high-performance reference counting library for C++.  It provides
smart pointer types with performance that greatly exceeds that of
`std::shared_ptr` and `boost::atomic_shared_ptr`, for example; in
particular, FRC's reference-counted pointer types excel in concurrent
scenarios (such as concurrent data structures).  A research paper describing
FRC in detail is linked at the bottom of this file.

## Organization

The repository consists of three main components:

  * The FRC library
  * Unit tests for the FRC library and supporting components
  * Benchmarks for the FRC library

Source code for the tests is located under the `test` folder, and source code
for the benchmarks is located under the `benchmark` folder.  Source code for
the FRC library is located under the `src` directory.

All build files (including binary output, CMake build scripts, Makefiles, etc.)
are located under the `build` directory.  Each of the components of the
repository has its own subdirectory and associated `CMakeLists.txt` under the
build directory.

The root-level `cmake` directory contains CMake helper routines, including the
common compiler flags used for compiling the projects.

The root-level `lib` directory contains scripts (and, when the FRC library is
built, the corresponding headers/libraries) for downloading and building
dependent libraries, such as Boost and Google Test (the latter being used for
managing tests and benchmarks).

## Dependencies

FRC automatically downloads and builds certain dependencies like Boost.
However, certain other dependencies may need to be installed.  For example,
in order to run `apply_style.sh`, you should first install the following
dependencies (written here as a one-line command for Debian-like operating
systems):
```bash
sudo apt-get install astyle
```

## Contributing

We welcome contributions to the FRC project.  Any new commits should be sure to
adhere to our code style; please run `./apply_style.sh` each time you are about
to make a commit in order to automatically enforce our code style.

## License

Please see the included [COPYING.md](COPYING.md).

## Using in a Research Project

If you use FRC in a research project, please cite our paper (below), and please
send us a citation for your paper!

> Charles Tripp, David Hyde, and Benjamin Grossman-Ponemon. 2018.
> FRC: A High-Performance Concurrent Parallel Deferred Reference Counter for C++.
> In Proceedings of the 2018 ACM SIGPLAN International Symposium on Memory Management
> (ISMM '18). ACM, New York, NY, USA, 14-28.
> https://doi.org/10.1145/3210563.3210569

