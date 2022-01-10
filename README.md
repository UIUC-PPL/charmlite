# CharmLite

Status:
- ~~Requires that `${env:CHARM_HOME}` points to a valid Charm++ build~~.
    - The lightest possible build is:
    `./build AMPI-only <triplet> <compiler>? --with-production -DCSD_NO_IDLE_TRACING=1 -DCSD_NO_PERIODIC=1`
    - Binaries should be compiled with `-fno-exceptions -fno-unwind-tables` to further minimize overheads.
    - Requires a valid Charm++ while building Charmlite (can be appended to `CMAKE_PREFIX_PATH`)
- Only tested with non-SMP builds, SMP builds ~~currently crash~~:
    - ~~This can be fixed by correctly isolating globals as Csv/Cpv.~~
    - Probably fixed but needs more testing.
- Uses Hypercomm distributed tree creation scheme for chare-array collectives:
    - [Google doc write-up.](https://docs.google.com/document/d/1hv-9qm1dXR8R1VJXgtyFHuhTUoa_izrm-jDXPqqkpas/edit?usp=sharing)
    - [Hypercomm implementation.](https://github.com/jszaday/hypercomm/blob/main/include/hypercomm/tree_builder/tree_builder.hpp)
- Add support for "location records" that indicate migratibility.
    - How should users specify whether elements can/not migrate?

## Build Instructions:

To build charmlite, the following dependecies are required:
- ~~A valid charm install and `${env:CHARM_HOME}` set to it.~~
- A cmake version higher than or equal to 3.16
- A C++11 conforming compiler

Charmlite doesn't support in-source tree builds. The easiest way to build
charmlite is as follows:-

```
$ git clone https://github.com/UIUC-PPL/charmlite
$ cd charmlite && mkdir build && cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<installation directory> -DCMAKE_PREFIX_PATH=<Charm++ installation directory> ..
$ make -j$(nproc)
$ make install
```

Other than the default cmake flags, Charmlite supports various options that
the user can enable using cmake:-
1. CHARMLITE_ENABLE_EXAMPLES - Enables building examples (default: ON)
2. CHARMLITE_ENABLE_TESTS - Enables building and testing tests (default: OFF)
3. CHARMLITE_ENABLE_BENCHMARKS - Enables building and testing benchmarks (default: OFF)
4. CHARMLITE_BENCHMARK_PE - PE argument passed to tests (default: 2)
5. CHARMLITE_BENCHMARK_PPN - PEs per node passed to tests (default: 2)

## Using Charmlite in dependant applications:

Users can use charmlite in an application that depends on it. A basic example
is as follows:-

Consider the following basic directory structure:-

```
$ tree my_application
my_application
├── CMakeLists.txt
└── pgm.cpp
```

The following inclusions to CMakeLists.txt are crucial:-

```
cmake_minimum_required(VERSION 3.16)

project(CharmLite CXX)

find_package(CharmLite)

add_executable(pgm pgm.cpp)
target_link_libraries(pgm PUBLIC CharmLite::charmlite)
```

The notable additions to CMakeLists.txt here are `find_package(CharmLite)`
and `target_link_libraries(<target> PUBLIC CharmLite::charmlite)`. 
**find_package** tries to find charmlite installation while linking
**CharmLite::charmlite** ensures proper headers and libraries pertaining
to charmlite are included and linked.

To build:

```
$ make build && cd build
$ cmake -DCMAKE_PREFIX_PATH=<path to charmlite installation> ..
$ make
```

Adding installation path to **CMAKE_PREFIX_PATH** allows cmake to find
charmlite and set various charmlite related cmake flags.

## Overall

Overall... need more examples; feel free to _try_ porting your favorite example. Agenda:
- ~~Jacobi2d~~
