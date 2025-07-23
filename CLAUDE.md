## Project Organization and Documentation
- Read the summary.md files in any directory you are inspecting to optimize and streamline your approach.
- For each directory inspected or modified, maintain a summary.md markdown file that lists the files in the directory and a concise one-line description of the file. For complex or important source files, also list with one-line summaries any critically important or hard-to-find class, namespace, struct, enum, object, interface, API, salient functions and global variables or properties. At the end of the file list recent changes. At the top of the file, summarize the contents and purpose of the directory in a one-line summary. Keep token counts in summary files low.

## Project Build and Development
- This project uses CMake.
- The primary xrc CMake file is @xrc/src/CMakeLists.txt
- Unit tests have the "_test.c" or "_test.cpp" suffix and are typically stored alongside the code they test.
- Parameters to function calls, declarations, and definitions are listed each on a separate line except for very simple and short functions.
- Parameters in function calls and definitions are commented with a comment on the same line as the parameter, avoiding redundancies such as listing each parameter name twice and making maintenance simpler.
- The development system has 64 cores and 128 SMT threads. Use these threads when compiling, debugging, running commands, and any other time it will help. 
- Always use CMake, never build directly or via make. Do not leave temporary files or build artifacts around. Do not add temporary files, build artifacts, or binaries to git. Tests are built via the CMake system. Files ending in _test.c or _test.cpp will be built into tests automatically.

## Compiling XRC Tests
### Using CMake (recommended):
```bash
cd xrc/src/xrc
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cd build && make <test_name>
```
- *ALWAYS* USE CMAKE to build tests! 
- Do not create loose binaries or other temporary files in the source directories.


### Key compilation notes:
- Include paths: `-I../../.. -I../..` for xrc headers
- Link with: `-llibxrc -latomic -lnuma -lm -pthread`
- For btree tests, include `btree.c` in compilation
- For tests using barriers, include `../util/barrier.c`
- The CMake build automatically handles test discovery and dependencies