# patton: hardware-sensitive parallel programming in C++

[![Language](https://img.shields.io/badge/language-C%2B%2B20%20-blue)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
 [![License](https://img.shields.io/badge/license-Boost-green)](https://opensource.org/licenses/BSL-1.0)
 [![Build Status](https://dev.azure.com/moritzbeutel/patton/_apis/build/status/mbeutel.patton?branchName=master)](https://dev.azure.com/moritzbeutel/patton/_build/latest?definitionId=10&branchName=master)
 [![Azure DevOps tests](https://img.shields.io/azure-devops/tests/moritzbeutel/patton/10)](https://dev.azure.com/moritzbeutel/patton/_testManagement/runs)


***patton*** is a library for hardware-sensitive parallel programming in C++.


## Contents

- [Example usage](#example-usage)
- [In a nutshell](#in-a-nutshell)
- [License](#license)
- [Dependencies](#dependencies)
- [Compiler and platform support](#compiler-and-platform-support)
- [Installation and use](#installation-and-use)
- [Features](#features)
- [Reference documentation](doc/Reference.md)
- [Version semantics](#version-semantics)


## Example usage

```c++
#include <thread>
#include <iostream>

#include <patton/thread.hpp>

int main()
{
    std::cout << "Number of hardware threads: " << std::thread::hardware_concurrency() << "\n";
    std::cout << "Number of cores: " << patton::physical_concurrency() << "\n";
}
```


## License

*patton* uses the [Boost Software License](LICENSE.txt).


## Dependencies

* [gsl-lite](https://github.com/gsl-lite/gsl-lite), an implementation of the [C++ Core Guidelines Support Library](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-gsl)
* optional (for testing only): [*Catch2*](https://github.com/catchorg/Catch2)


## Compiler and platform support

*patton* requires a compiler and standard library conforming with the C++20 standard.

*patton* currently supports Microsoft Windows and Linux and has partial support for MacOS (no thread pinning).

The following compilers are officially supported (that is, part of
[our CI pipeline](https://dev.azure.com/moritzbeutel/patton/_build/latest?definitionId=10&branchName=master)):

- Microsoft Visual C++ 19.3 and newer (Visual Studio 2022 and newer)
- GCC 12 and newer with libstdc++ (tested on Linux and MacOS)
- Clang 19 and newer with libc++ (tested on Windows and Linux)
- AppleClang 16.0 and newer with libc++ (Xcode 16.0 and newer)


## Installation and use

The recommended way to consume *patton* in your CMake project is to use `find_package()` to locate the package `patton`
and `target_link_libraries()` to link to the imported target `patton::patton`:

```cmake
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(my-program LANGUAGES CXX)

find_package(patton 1.0 REQUIRED)

add_executable(my-program main.cpp)
target_compile_features(my-program PRIVATE cxx_std_20)
target_link_libraries(my-program PRIVATE patton::patton)
```

*patton* may also be obtained with [CPM](https://github.com/cpm-cmake/CPM.cmake):
```cmake
CPMFindPackage(NAME patton GIT_TAG master GITHUB_REPOSITORY mbeutel/patton)
```


## Features

*patton* provides the following components:

- [Allocators](doc/Reference.md#allocators) with user-defined alignment and element initialization
- [Containers](doc/Reference.md#containers) with user-defined alignment
- Basic [hardware information](doc/Reference.md#hardware-information) (page size, cache line size, number of cores)
- [`thread_squad`](doc/Reference.md#thread_squad), a simple thread pool

For more information, please refer to the [reference documentation](doc/Reference.md).


## Version semantics

*patton* follows [Semantic Versioning](https://semver.org/) guidelines. We maintain [API](https://en.wikipedia.org/wiki/Application_programming_interface) and
[ABI](https://en.wikipedia.org/wiki/Application_binary_interface) compatibility and avoid breaking changes in minor and patch releases.

Development of *patton* happens in the `master` branch. Versioning semantics apply only to tagged releases: there is no stability guarantee between individual
commits in the `master` branch, that is, anything added since the last tagged release may be renamed, removed, or have the semantics changed without further notice.

A minor-version release will be compatible (in both ABI and API) with the previous minor-version release. Thus, once a change is released, it becomes part of the API.
