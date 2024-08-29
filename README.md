# Thread Pool

In the chapter on multithreaded algorithms in Introduction to Algorithms, by CLRS, the first example is a multithreaded application of recursively finding the nth Fibonacci. But if you run it in C++, and n were to be huge, the computer would simply crash at some point.

So, I created a thread pool to run many tasks on a limited number of threads, with any type of function, and a simple interface.

However, it is much more difficult to do it with recursive Fibonacci, threads create threads. 

At some point, the root Fibonacci call will sit in the worker thread, waiting on the leaf to finish running. But if the height of the recursion tree is massive, so big it's too big for the thread pool, that the leaf function is waiting to be run, then you have a deadlock. The root has to finish by waiting for the leaf, but the leaf can't run till some other thread finishes with a function, all of which are often higher than the leaf. 

Still trying to figure a solution to resolve this deadlock, with potential solutions being:
1. Reverse the queue to a stack, so leaves run earlier
2. Prioritize tasks