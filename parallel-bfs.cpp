#include "ThreadPool.h"
#include <iostream>
#include <chrono>

// Example of BFS with parallelization using a thread pool
void parallel_bfs(int start_node, const std::vector<std::vector<int>>& graph, ThreadPool& pool) {
    std::vector<bool> visited(graph.size(), false);
    visited[start_node] = true;
    std::vector<int> frontier = { start_node };

    while (!frontier.empty()) {
        std::vector<int> next_frontier;

        // Parallelize processing of all nodes at the current level
        std::vector<std::future<void>> futures;
        for (int node : frontier) {
            futures.push_back(pool.enqueue([node, &graph, &visited, &next_frontier]() {
                for (int neighbor : graph[node]) {
                    if (!visited[neighbor]) {
                        visited[neighbor] = true;
                        next_frontier.push_back(neighbor);
                    }
                }
            }));
        }

        // Wait for all tasks to complete before moving to the next level
        for (auto& f : futures) {
            f.get();
        }

        // Move to the next level
        frontier = std::move(next_frontier);
    }
}

int main() {
    // Example graph (adjacency list)
    std::vector<std::vector<int>> graph = {
        {1, 2},       // Node 0
        {0, 3, 4},    // Node 1
        {0, 4},       // Node 2
        {1, 5},       // Node 3
        {1, 2, 5},    // Node 4
        {3, 4}        // Node 5
    };

    ThreadPool pool(std::thread::hardware_concurrency());  // 4 threads in the pool

    auto start = std::chrono::high_resolution_clock::now();
    parallel_bfs(0, graph, pool);  // Start BFS from node 0
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Parallel version took " << duration.count() << " milliseconds.\n";
    return 0;
}