#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include "../TaskScheduler.h"

// Merge function
void merge(std::vector<int>& arr, int l, int mid, int r) {
    std::vector<int> temp;
    int i = l, j = mid;
    while (i < mid && j < r) {
        if (arr[i] <= arr[j]) temp.push_back(arr[i++]);
        else temp.push_back(arr[j++]);
    }
    while (i < mid) temp.push_back(arr[i++]);
    while (j < r) temp.push_back(arr[j++]);
    std::copy(temp.begin(), temp.end(), arr.begin() + l);
}

// Recursive parallel merge sort
TaskGraphScheduler::TaskHandle<void> merge_sort(
    std::vector<int>& arr, int l, int r,
    TaskGraphScheduler& sched, int cutoff = 16
) {
    if (r - l <= cutoff) {
        return sched.submit<void>([&arr, l, r] {
            std::sort(arr.begin() + l, arr.begin() + r);
        });
    } else {
        int mid = l + (r - l) / 2;

        // Recursively spawn left and right tasks
        auto left  = merge_sort(arr, l, mid, sched, cutoff);
        auto right = merge_sort(arr, mid, r, sched, cutoff);

        // Merge task depends on left and right
        return sched.submit_with_deps<void>(
            [&arr, l, mid, r] { merge(arr, l, mid, r); },
            std::vector<std::shared_ptr<TaskGraphScheduler::ITaskNode>>{ left.node, right.node }
        );
    }
}

int main() {
    const int N = 1 << 16;  // array size
    std::vector<int> data(N);

    // Fill with random numbers
    std::mt19937 rng(1234);
    std::uniform_int_distribution<int> dist(0, 1000000);
    for (auto& x : data) x = dist(rng);

    // Create thread pool and scheduler
    ThreadPool pool(std::thread::hardware_concurrency());
    TaskGraphScheduler sched(pool);

    // Run parallel merge sort
    auto rootTask = merge_sort(data, 0, data.size(), sched);
    rootTask.get_future().get();  // wait for completion

    // Verify sorted
    if (std::is_sorted(data.begin(), data.end()))
        std::cout << "Array sorted successfully!\n";
    else
        std::cout << "Sorting failed!\n";

    return 0;
}