# Thread Pool

In this project, I built a thread pool in C++ with `std::thread`, and added handling for inter-dependent tasks using C++ [coroutines](https://www.scs.stanford.edu/~dm/blog/c++-coroutines.html),

## Background

Computing the nth Fibonacci using fork-join parallelism is a classic example of a parallel algorithm. But if a thread is created at each level of recursion and n were to be huge, the computer would simply crash at some point.

So, I created a thread pool inside the file `ThreadPool.h` to allow a program to run many tasks on a limited number of threads, with any type of function (even variadic arguments!), and a simple interface. 

Moreover, the size of the pool is automatically optimized using the C++ `std::thread::hardware_concurrency()` which returns the number of logical processors. This is not necessarily the number of cores, since a single hardware core may include multiple execution contexts.

However, the pool does not work with a Fibonacci calculation working like this:

```C++
int fibonacci(int n) {
  auto res = pool.enqueue(fibonacci, n-1);
  return res.get() + fibonacci(n-2);
}
```
This is because the root function of any Fibonacci recursion tree sits in the pool until it is done, which occurs when all its children are done. But if the height of the recursion tree is massive, so big it's too big for the thread pool, then a deadlock will result. 

Why? The root has to finish by waiting for the leaf, but the leaf can't run till some other thread finishes running, all of which are often higher than the leaf. But those higher functions are waiting for the task sitting in the queue to run. They're waiting for each other, hence the deadlock.

while I didn't try memoizing Fibonacci, I believe a deadlock would have occured either way, since a bigger value would have led to many leaf calculations.

At first, I tried to reverse the queue to a stack so leaf functions ran earlier. Didn't help. Still deadlocked, although a little later. Then I tried prioritizing tasks, which was also insanely hard and never worked.

Then I realized that parent functions had to be able to exit the execution context to allow the recursive calls to run in the pool. In other words, they had to suspend. So I searched for quite a long time for such things, and found the solution in...coroutines!

## The Solution

Coroutines were introduced in C++20, and allow for suspension & resumption of execution. They are also more lightweight than threads as they are not managed by the OS, so suspensions incur less overhead.

Execution states are saved in the heap when suspended and are managed by coroutine handles that point to the context. 

The coroutine handle, of which there will be multiple for thread pools, is stored in a coroutine type (that is user-defined) that provides an interface to call coroutines.

Nested within the coroutine type is the promise type, that represents the coroutine state that is changed with calls to operators such as `co_await`, `co_yield`, and `co_return`.

More notes are to be continued. 
