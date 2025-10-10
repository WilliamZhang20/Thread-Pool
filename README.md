# Task Execution Engine

This project is a custom task execution library in C++, consisting of a thread pool and task scheduler.

It also happens that a similar system is an upcoming feature in [C++26](https://en.cppreference.com/w/cpp/execution.html), so it is interesting to try implementing the idea from scratch.

## Getting Started

The files in the home directory contain the components of the scheduler:
1. Lock-Free Queue to handle each processor's workload
2. Thread Pool with automatically optimized number of workers with `std::thread::hardware_concurrency()`. This number of workers is a balance between the number of physical cores and the total number of hardware threads. Each worker has a task queue and can steal from others to balance workload.
3. Graph scheduler for dependent tasks, applying the thread pool to universal usage. The user can just set dependencies between task handles and the scheduler will handle them.

## Profiling Results

The `test` directory contains benchmarks to stress test the scheduler, which are run inside Intel VTune profiler to assess hardware footprint and usage.

The latest thread pool test yields 85% logical core usage.

## Previous Iterations

The project is in its third iteration. The previous ones were:
1. A simple thread pool with a global queue to process groups of independent tasks in parallel.
2. A solution to handle task dependencies with C++20 [coroutines](https://www.scs.stanford.edu/~dm/blog/c++-coroutines.html).

The first iteration could not handle dependent tasks.

The second iteration's performance efficiency was bad. The heavy context switching through `co_await` and `co_return` operations between threads significantly degraded performance in comparison to the sequential alternative.