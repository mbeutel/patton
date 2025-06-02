# Reference documentation

## Contents

- [Allocators](#allocators) with user-defined alignment and element initialization
- [Containers](#containers) with user-defined alignment
- Basic [hardware information](#hardware-information) (page size, cache line size, number of cores)
- A configurable [thread pool](#thread-pools)

All symbols defined here reside in the namespace `patton`.


## Allocators

Header file: `<patton/memory.hpp>`

- [`default_init_allocator<>`](#default_init_allocator): allocator adaptor which default-initializes elements
- [`zero_init_allocator<>`](#zero_init_allocator): allocates zero-initialized memory
- [`aligned_allocator<>`](#aligned_allocator): allocates with user-defined alignment
- [`aligned_allocator_adaptor<>`](#aligned_allocator_adaptor): allocator adaptor which guarantees user-defined alignment
- [`page_allocator<>`](#page_allocator): allocates with page alignment
- [`large_page_allocator<>`](#page_allocator): allocates with large page alignment
- [Allocator support functions and traits](#allocator-support-functions-and-traits)

### `default_init_allocator<>`

Allocator adaptor that interposes `construct()` calls to convert [value initialization](https://en.cppreference.com/w/cpp/language/value_initialization.html)
into [default initialization](https://en.cppreference.com/w/cpp/language/default_initialization.html).

```c++
template <typename T, typename A = std::allocator<T>>
class default_init_allocator : public A { ... };
```

For POD types, default initialization amounts to no initialization. This may be more efficient if the memory will be overwritten anyway.
It may also be necessary for [NUMA](https://en.wikipedia.org/wiki/Non-uniform_memory_access)-sensitive page placement when relying on the
[first-touch policy](https://hpc-wiki.info/hpc/NUMA).


### `zero_init_allocator<>`

Allocator that always returns zero-initialized memory. Implemented in terms of [`calloc()`](https://en.cppreference.com/w/c/memory/calloc).

```c++
template <typename T>
class zero_init_allocator { ... };
```


### `aligned_allocator<>`

Allocator that aligns memory allocations for the given alignment using the default allocator, that is, global [`operator new()`](https://en.cppreference.com/w/cpp/memory/new/operator_new)
with [`std::align_val_t`](https://en.cppreference.com/w/cpp/memory/new/align_val_t.html).

```c++
template <typename T, std::size_t Alignment>
class aligned_allocator { ... };
```
ᅟ
`aligned_allocator<>` supports [special alignment values](#special-alignment-values) such as `cache_line_alignment`.
Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.

### `aligned_allocator_adaptor<>`

Allocator adaptor that aligns memory allocations for the given alignment.

```c++
template <typename T, std::size_t Alignment, typename A>
class aligned_allocator_adaptor : ... { ... };
```

`aligned_allocator_adaptor<> `supports [special alignment values](#special-alignment-values) such as `cache_line_alignment`.
Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.

### `page_allocator<>`

Obtains page-granular allocations directly from the operating system.

```c++
template <typename T>
class page_allocator { ... };
```

On Linux, transparent huge pages are suppressed for allocations made by this allocator.


### `large_page_allocator<>`

Allocates elements with large-page alignment. Uses transparent huge pages on Linux and explicit large page allocation on Windows.

```c++
template <typename T>
class large_page_allocator { ... };
```

Note that processes on Windows need to obtain `SeLockMemoryPrivilege` in order to use large pages,
cf. https://docs.microsoft.com/en-us/windows/win32/memory/large-page-support.


### Allocator support functions and traits

- [Special alignment values](#special-alignment-values)
- [Alignment checking](#alignment-checking)
- [`allocate_unique<>()` and `allocator_deleter<>`](#allocate_unique-and-allocator_deleter)

#### Special alignment values

As per [https://en.cppreference.com/w/cpp/language/objects.html](https://en.cppreference.com/w/cpp/language/objects.html#Alignment),
an alignment requirement "is a nonnegative integer value (of type [std::size_t](https://en.cppreference.com/w/cpp/types/size_t.html),
and always a power of two)". The allocators and containers in *patton* which accept an alignment value will also respect
the following special alignment values:

Special alignment value | Meaning              |
-----------------------:|:---------------------|
`large_page_alignment`  | Large page alignment |
`page_alignment`        | Page alignment       |
`cache_line_alignment`  | Cache line alignment |

#### Alignment checking

The following helper functions can be used to check whether an alignment is satisfied:

```c++
constexpr bool provides_static_alignment(
    std::size_t alignmentProvided,
    std::size_t alignmentRequested) noexcept;

bool provides_dynamic_alignment(
    std::size_t alignmentProvided,
    std::size_t alignmentRequested) noexcept;
```

The alignments corresponding to the special alignment values `large_page_alignment`, `page_alignment`, and `cache_line_alignment`
are not known until runtime. Therefore, the compile-time predicate `provides_static_alignment()` can return `true` only if the
requested special alignment is stated explicitly by the provided alignment:

```c++
std::size_t pageSize = hardware_page_size();

    // false because alignment cannot be guaranteed statically
bool isStaticCLAligned = provides_static_alignment(pageSize, cache_line_alignment);

    // true because page size is a multiple of cache line size
bool isCLAligned = provides_dynamic_alignment(pageSize, cache_line_alignment);

    // true because page size is a multiple of cache line size, and because special values are used
bool isStaticCLAligned2 = provides_static_alignment(page_alignment, cache_line_alignment);
```

This is not necessary when calling the runtime predicate `provides_dynamic_alignment()`, which can look up the alignments
corresponding to the special alignment values `large_page_alignment`, `page_alignment`, and `cache_line_alignment`:

```c++
    // true because page size is a multiple of cache line size
bool isCLAligned = provides_dynamic_alignment(pageSize, cache_line_alignment);
```

The static alignment guaranteed by an allocator `A` can be queried with `aligned_allocator_traits<A>`:

```c++
template <typename A>
struct aligned_allocator_traits
{
    static constexpr bool provides_static_alignment(std::size_t a) noexcept;
};
```

#### `allocate_unique<>()` and `allocator_deleter<>`

In analogy to [`std::allocate_shared<>()`](https://en.cppreference.com/w/cpp/memory/shared_ptr/allocate_shared),
*patton* provides a set of function template overloads `allocate_unique<>()` an a custom deleter class `allocator_deleter<>`:

```c++
template <typename T, typename A>
class allocator_deleter { ... };
template <typename T, typename A>
class allocator_deleter<T[], A> { ... };
template <typename T, std::ptrdiff_t N, typename A>
class allocator_deleter<T[N], A> { ... };

template <typename T, typename A, typename... ArgsT>
allocator_deleter<T, A>
allocate_unique(A alloc, ArgsT&&... args);   // (1)
template <typename ArrayT, typename A>
allocator_deleter<ArrayT, A>
allocate_unique(A alloc);                    // (2)
template <typename ArrayT, typename A>
allocator_deleter<ArrayT, A>
allocate_unique(A alloc, std::size_t size);  // (3)
```

Overload (1) allocates an object of type `T` with the given allocator, constructs it with the given arguments, and returns a
`std::unique_ptr<>` to the object. It participates in overload resolution only for non-array types `T`.

Example:

```c++
auto p = allocate_unique<float>(MyAllocator<float>{ }, 3.14159f);
// returns `std::unique_ptr<float, allocator_deleter<float, MyAllocator<float>>>`
```

Overload (2) allocates a fixed-size array of type `ArrayT` with the given allocator, default-constructs the elements, and returns a
`std::unique_ptr<>` holding the array. It participates in overload resolution only for fixed-size array types `ArrayT`.

Example:

```c++
auto p = allocate_unique<float[42]>(MyAllocator<float>{ });
// returns `std::unique_ptr<float[42], allocator_deleter<float[42], MyAllocator<float>>>`
```

Overload (3) allocates an array of type `ArrayT` and size `size` with the given allocator, default-constructs the elements, and
returns a `std::unique_ptr<>` holding the array. It participates in overload resolution only for unbounded array types `ArrayT`.

Example:

```c++
auto p = allocate_unique<float[]>(MyAllocator<float>{ }, 42);
// returns `std::unique_ptr<float[], allocator_deleter<float[], MyAllocator<float>>>`
```


## Containers

Header file: `<patton/buffer.hpp>`

- [`aligned_buffer<>`](#aligned_buffer): buffer with aligned elements
- [`aligned_row_buffer<>`](#aligned_row_buffer): two-dimensional buffer with aligned rows

### `aligned_buffer<>`

Fixed-size buffer of potentially non-contiguous elements, where the alignment requirement specified by `Alignment` is satisfied
for every individual element.

```c++
template <typename T, std::size_t Alignment, typename A = aligned_allocator<T, Alignment>>
class aligned_buffer
{
    ...

public:
    using allocator_type = aligned_allocator_adaptor<T, Alignment | alignof(T), A>;

    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = T const*;
    using reference = T&;
    using const_reference = T const&;

    using iterator = /*implementation-defined*/;
    using const_iterator = /*implementation-defined*/;

    template <typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    constexpr aligned_buffer() noexcept;
    aligned_buffer(allocator_type _alloc) noexcept;

    template <typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    explicit aligned_buffer(std::size_t _size);
    template <typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    explicit aligned_buffer(std::size_t _size, T const& _value);
    explicit aligned_buffer(std::size_t _size, A _alloc);
    explicit aligned_buffer(std::size_t _size, T const& _value, A _alloc);

    template <typename... Ts,
              typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    explicit aligned_buffer(std::size_t _size, std::in_place_t, Ts&&... _args);
    template <typename... Ts>
    explicit aligned_buffer(std::size_t _size, A _alloc, std::in_place_t, Ts&&... _args);

    constexpr aligned_buffer(aligned_buffer&& rhs) noexcept;
    constexpr aligned_buffer& operator =(aligned_buffer&& rhs) noexcept;
    ~aligned_buffer();

    allocator_type get_allocator() const noexcept;

    std::size_t size() const noexcept;
    reference operator [](std::size_t i);
    const_reference operator [](std::size_t i) const;

    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    iterator end() noexcept;
    const_iterator end() const noexcept;

    constexpr bool empty() const noexcept;

    reference front();
    const_reference front() const;
    reference back();
    const_reference back() const;
};
```

Example:
```c++
auto threadData = aligned_buffer<ThreadData, cache_line_alignment>(numThreads);
// every `threadData[i]` has cache-line alignment => no false sharing
```

`aligned_buffer<>` supports [special alignment values](#special-alignment-values) such as `cache_line_alignment`.
Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.


### `aligned_row_buffer<>`

Fixed-size two-dimensional buffer where the first element in each row satisfies the alignment requirement specified
by `Alignment`. The elements in a row are stored contiguously.

```c++
template <typename T, std::size_t Alignment, typename A = aligned_allocator<T, Alignment>>
class aligned_row_buffer
{
    ...

public:
    using allocator_type = aligned_allocator_adaptor<T, Alignment | alignof(T), A>;

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = std::span<T>;
    using const_reference = std::span<T const>;

    using iterator = /*implementation-defined*/;
    using const_iterator = /*implementation-defined*/;

    template <typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    aligned_row_buffer() noexcept;
    aligned_row_buffer(allocator_type _alloc) noexcept;
    template <typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols);
    template <typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, T const& _value);
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, A _alloc);
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, T const& _value, A _alloc);
    template <typename... Ts,
              typename U = int, std::enable_if_t<std::is_default_constructible_v<allocator_type>, U> = 0>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, std::in_place_t, Ts&&... _args);
    template <typename... Ts>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, A _alloc, std::in_place_t, Ts&&... _args);

    constexpr aligned_row_buffer(aligned_row_buffer&& rhs) noexcept;
    constexpr aligned_row_buffer& operator =(aligned_row_buffer&& rhs) noexcept;

    ~aligned_row_buffer();

    allocator_type get_allocator() const noexcept;

    std::size_t rows() const noexcept;
    std::size_t columns() const noexcept;

    std::size_t size() const noexcept;
    std::span<T> operator [](std::size_t i);
    std::span<T const> operator [](std::size_t i) const;

    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    iterator end() noexcept;
    const_iterator end() const noexcept;

    constexpr bool empty() const noexcept;

    reference front();
    const_reference front() const;
    reference back();
    const_reference back() const;
};
```

Example:
```c++
auto threadData = aligned_row_buffer<float, cache_line_alignment>(rows, cols);
// every `threadData[i][0]` has cache-line alignment => no false sharing
```

`aligned_row_buffer<>` supports [special alignment values](#special-alignment-values) such as `cache_line_alignment`.
Multiple alignment requirements can be combined using bitmask operations, e.g. `cache_line_alignment | alignof(T)`.


## Hardware information



### Cache line and page sizes

Header file: `<patton/new.hpp>`

The function `hardware_large_page_size()` reports the operating system's large page size in bytes:
```c++
std::size_t hardware_large_page_size() noexcept;
```
`hardware_large_page_size()` may return 0 if large pages are not available or not supported.

The function `hardware_page_size()` reports the operating system's page size in bytes:
```c++
std::size_t hardware_page_size() noexcept;
```

The function `hardware_cache_line_size()` reports the CPU architecture's cache line size in bytes:
```c++
std::size_t hardware_cache_line_size() noexcept;
```


### Concurrency

Header file: `<patton/thread.hpp>`

#### `physical_concurrency()`

The function `physical_concurrency()` reports the number of concurrent physical cores available:

```c++
unsigned physical_concurrency() noexcept;
```

Unlike [`std::thread::hardware_concurrency()`](https://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency.html),
this only returns the number of cores, not the number of hardware threads. On systems with simultaneous multithreading
("hyper-threading") enabled, `std::thread::hardware_concurrency()` typically returns some multiple of `physical_concurrency()`.


#### `physical_core_ids()`

The function `physical_core_ids()` returns a list of thread ids, where each thread is situated on a distinct physical core:
```c++
std::span<int const> physical_core_ids() noexcept;
```
It can be used to configure thread affinity in [`thread_squad`](#thread-pools) if no simultaneous multithreading
("hyper-threading") is desired.

`physical_core_ids()` returns an empty span if thread affinity is not supported by the OS.


## Thread pools

Header file: `<patton/thread_squad.hpp>`

- [`thread_squad::params`](#thread_squad-params)
- [`thread_squad` member functions](#thread_squad-member-functions)
- [`thread_squad::task_context`](#thread_squad-task_context)
- [Examples](#examples)

A `thread_squad` is a simple thread pool with support for thread core affinity:

```c++
class thread_squad
{
    ...

public:
        // Thread squad parameters.
    struct params;

        // State passed to tasks that are executed in thread squad.
    class task_context;

public:
    explicit thread_squad(params const& p);

    thread_squad(thread_squad&&) noexcept = default;
    thread_squad& operator =(thread_squad&&) noexcept = default;

    thread_squad(thread_squad const&) = delete;
    thread_squad& operator =(thread_squad const&) = delete;

    ... // member functions are specified below
};
```

### `thread_squad::params`

A thread squad is constructed with a parameter argument of type `thread_squad::params`, which is defined as follows:

```c++
struct thread_squad::params
{
    int num_threads = 0;
    bool pin_to_hardware_threads = false;
    bool spin_wait = false;
    int max_num_hardware_threads = 0;
    std::span<int const> hardware_thread_mappings = { };
};
```

- `num_threads` indicates how many threads to fork. A value of 0 indicates "as many as hardware threads are available"
  (cf. [`std::thread::hardware_concurrency()`](https://en.cppreference.com/w/cpp/thread/thread/hardware_concurrency.html)).

- `pin_to_hardware_threads` controls whether threads are pinned to hardware threads, that is, whether threads have a
  core affinity. This may helps maintain data locality.  
  *Note:* Core affinity is not currently supported on MacOS.

- `spin_wait` controls whether thread synchronization uses spin waiting with exponential backoff. This is often faster
  than wait-based synchronization, especially on highly parallel systems.

- `max_num_hardware_threads` limits the maximal number of hardware threads to pin threads to. A value of 0 indicates
  "as many as possible".  
  If `max_num_hardware_threads` is 0 and `hardware_thread_mappings` is non-empty, `hardware_thread_mappings.size()`
  is taken as the maximal number of hardware threads to pin threads to.  
  If `hardware_thread_mappings` is not empty, `max_num_hardware_threads` must not be larger than
  `hardware_thread_mappings.size()`.  
  Setting `max_num_hardware_threads` can be useful to increase reproducibility of synchronization and data race bugs
  by running multiple threads on the same core.

- `hardware_thread_mappings` maps thread indices to hardware thread ids. If empty, the thread squad uses thread indices
  as hardware thread ids.  
  If non-empty and if `max_num_hardware_threads == 0`, `hardware_thread_mappings.size()` is taken as the maximal
  number of hardware threads to pin threads to.


### `thread_squad` member functions

`thread_squad` has the following member functions:

- [`thread_squad::num_threads()`](#thread_squad-num_threads): returns number of threads held by the thread squad
- [`thread_squad::run()`](#thread_squad-run): concurrently executes an action
- [`thread_squad::transform_reduce()`](#thread_squad-transform_reduce): concurrently executes a transform–reduce operation
- [`thread_squad::transform_reduce_first()`](#thread_squad-transform_reduce_first): concurrently executes a transform–reduce operation without initial value


#### `thread_squad::num_threads()`

The member function `num_threads()` returns number of threads held by the thread squad:
```c++
int thread_squad::num_threads() const;
```


#### `thread_squad::run()`

The member function template `run(action, concurrency)` executes the given action on `concurrency` threads and waits until all tasks have
run to completion:
```c++
template <std::invocable<task_context&> ActionT>
requires std::copy_constructible<ActionT>
void thread_squad::run(ActionT action, int concurrency = -1);
```

`concurrency` must not exceed the number of threads in the thread squad. A value of -1 indicates that all available
threads shall be used.

The thread squad makes a dedicated copy of `action` for every participating thread and invokes it with a thread-specific
[`task_context&`](#thread_squad-task_context) argument. If `action` throws an exception,
[`std::terminate()`](https://en.cppreference.com/w/cpp/error/terminate.html) is called.


#### `thread_squad::transform_reduce()`

The member function template `transform_reduce_first(transformFunc, reduceOp, concurrency)` runs `transformFunc` on `concurrency` threads
and waits until all tasks have run to completion, then returns the result reduced with the `reduceOp` operator:
```c++
template <std::invocable<task_context&> TransformFuncT, typename ReduceOpT>
requires std::copy_constructible<TransformFuncT> &&
         std::copy_constructible<ReduceOpT> &&
         std::copyable<std::invoke_result_t<TransformFuncT, task_context&>>
auto thread_squad::transform_reduce(
    TransformFuncT transformFunc,
    std::invoke_result_t<TransformFuncT, task_context&> init,
    ReduceOpT reduceOp,
    int concurrency = -1);
```

`concurrency` must not exceed the number of threads in the thread squad. A value of -1 indicates that all available
threads shall be used.

The thread squad makes a dedicated copy of `transformFunc` and `reduceOp` for every participating thread. `transformFunc`
is invoked with a thread-specific [`task_context&`](#thread_squad-task_context) argument. If either of `transformFunc` or
`reduceOp` throws an exception, [`std::terminate()`](https://en.cppreference.com/w/cpp/error/terminate.html) is called.


#### `thread_squad::transform_reduce_first()`

The member function template `transform_reduce_first(transformFunc, reduceOp, concurrency)` runs `transformFunc` on `concurrency` threads
and waits until all tasks have run to completion, then returns the result reduced with the `reduceOp` operator:
```c++
template <std::invocable<task_context&> TransformFuncT, typename ReduceOpT>
requires std::copy_constructible<TransformFuncT> &&
         std::copy_constructible<ReduceOpT> &&
         std::copyable<std::invoke_result_t<TransformFuncT, task_context&>>
auto thread_squad::transform_reduce_first(
    TransformFuncT transformFunc,
    ReduceOpT reduceOp,
    int concurrency = -1);
```

`concurrency` must not be 0 and must not exceed the number of threads in the thread squad. A value of -1 indicates that
all available threads shall be used.

The thread squad makes a dedicated copy of `transformFunc` and `reduceOp` for every participating thread. `transformFunc`
is invoked with a thread-specific [`task_context&`](#thread_squad-task_context) argument. If either of `transformFunc` or
`reduceOp` throws an exception, [`std::terminate()`](https://en.cppreference.com/w/cpp/error/terminate.html) is called.

### `thread_squad::task_context`

The class `thread_squad::task_context` represents the state passed to tasks that are executed in thread squad.
It has the following member functions:

- [`task_context::thread_index()`](#task_context-thread_index): returns current thread index
- [`task_context::num_threads()`](#task_context-num_threads): returns number of currently executing threads
- [`task_context::synchronize()`](#task_context-synchronize): synchronizes threads which execute the current task
- [`task_context::reduce()`](#task_context-reduce): performs a reduction operation among currently executing threads
- [`task_context::reduce_transform()`](#task_context-reduce_transform): performs a reduction operation among currently executing threads followed by a synchronous transformation


#### `task_context::thread_index()`

The member function `thread_index()` returns the index of the thread currently executing:
```c++
int thread_squad::task_context::thread_index() const noexcept;
```

The thread index is a value greater than or equal to 0 and smaller than [`num_threads()`](#task_context-num_threads).


#### `task_context::num_threads()`

The member function `num_threads()` returns the number of concurrent threads currently executing the task:
```c++
int thread_squad::task_context::num_threads() const noexcept;
```


#### `task_context::synchronize()`

The member function `synchronize()` synchronizes all threads which execute the current task:
```c++
void thread_squad::task_context::synchronize() noexcept;
```

It is the responsibility of the task to ensure that synchronization operations such as `synchronize()`, `reduce_transform()`,
and `reduce()` are executed by all participating threads unconditionally and in the same order.


#### `task_context::reduce()`

The member function template `reduce(value, reduceOp)` synchronizes all threads which execute the
current task and computes the reduction of `value` for all threads with the reduction operation `reduceOp`:
```c++
template <std::copyable T, typename ReduceOpT, std::invocable<T> TransformFuncT>
auto thread_squad::task_context::reduce_transform(
    T value,
    ReduceOpT reduceOp,
    TransformFuncT transformFunc) noexcept;
```
The result of the reduction is communicated back to all threads and returned by the `reduce()` call.

The function object `reduceOp` is executed on the calling thread only.  
It is the responsibility of the task to ensure that synchronization operations such as `synchronize()`, `reduce_transform()`,
and `reduce()` are executed by all participating threads unconditionally and in the same order.  
If the function object `reduceOp` throws an exception, `std::terminate()` is called.


#### `task_context::reduce_transform()`

The member function template `reduce_transform(value, reduceOp, transformFunc)` synchronizes all threads which execute the
current task and computes the reduction of `value` for all threads with the reduction operation `reduceOp`:
```c++
template <std::copyable T, typename ReduceOpT, std::invocable<T> TransformFuncT>
auto thread_squad::task_context::reduce_transform(
    T value,
    ReduceOpT reduceOp,
    TransformFuncT transformFunc) noexcept;
```
The result of the reduction is transformed with the `transformFunc` operation, communicated back to all threads, and
returned by the `reduce_transform()` call.

The function objects `reduceOp` and `transformFunc` are executed on the calling thread only. `transformFunc` is executed
only on the thread which is the root of the synchronization operation.  
It is the responsibility of the task to ensure that synchronization operations such as `synchronize()`, `reduce_transform()`,
and `reduce()` are executed by all participating threads unconditionally and in the same order.  
If either of the function objects `transformFunc` or `reduceOp` throws an exception, `std::terminate()` is called.


### Examples

The following code uses a thread squad with the default configuration to concurrently execute
[`std::println()`](https://en.cppreference.com/w/cpp/io/println.html) statements on every hardware thread in the system:

```c++
#include <print>

#include <patton/thread_squad.hpp>

int main()
{
    patton::thread_squad({ }).run(
        [](patton::thread_squad::task_context& taskCtx)
        {
            std::println("Hello from thread {}", taskCtx.thread_index());
        });
}
```

One-time use of `thread_squad` can be unnecessarily wasteful because forking and joining threads can be very expensive. A thread
squad is meant to be reused; it maintains a pool of threads that go to sleep after processing a task and can be awakened on demand,
which is usually more efficient than joining them and forking new threads.

The following example uses a thread squad with pinned threads to concurrently execute `std::println()` statements on every
core, as opposed to every hardware thread:

```c++
#include <print>

#include <gsl-lite/gsl-lite.hpp>

#include <patton/thread_squad.hpp>

int main()
{
    auto threadSquad = patton::thread_squad({
        .num_threads = gsl_lite::narrow_failfast<int>(patton::physical_concurrency()),
        .pin_to_hardware_threads = true,
        .hardware_thread_mappings = patton::physical_core_ids()
    });
    threadSquad.run(
        [](patton::thread_squad::task_context& taskCtx)
        {
            std::println("Hello from thread {}", taskCtx.thread_index());
        });
}
```


The threads in a thread squad may communicate through reductions, as is demonstrated by the following example:

```c++
#include <print>
#include <functional>

#include <patton/thread_squad.hpp>

int main()
{
    auto threadSquad = patton::thread_squad({
        .num_threads = 4
    });
    threadSquad.run(
        [](patton::thread_squad::task_context& taskCtx)
        {
            int iThread = taskCtx.thread_index();
            int sum = taskCtx.reduce(iThread, std::plus<>{ });  // 0 + 1 + 2 + 3 = 6
            std::println("Hello from thread {}; the sum of all thread indices is {}", iThread, sum);
        });
}
```
