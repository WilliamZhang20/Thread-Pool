#include "../TaskScheduler.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <iostream>

double heavyComputation(int n) {
    // Simulate CPU-heavy work (burn cycles deterministically)
    double sum = 0.0;
    for (int i = 1; i < n; ++i) {
        sum += std::sin(i) * std::cos(i / 2.0);
    }
    return sum;
}

int main() {
    ThreadPool pool;              // 4 worker threads
    TaskScheduler scheduler(pool);
    TaskGraph graph;

    constexpr int layers = 10;       // depth of DAG
    constexpr int width  = 8000;       // number of tasks per layer

    std::vector<std::vector<TaskNode*>> nodes(layers);

    // Create tasks
    for (int l = 0; l < layers; ++l) {
        for (int w = 0; w < width; ++w) {
            auto* node = graph.addTask([l, w]() {
                // Simulate variable heavy work
                double result = heavyComputation(50000 + (l * 1000 + w * 100));
                // Uncomment for debug (not VTune):
                // std::printf("Task L%d-W%d result=%.3f\n", l, w, result);
            });
            nodes[l].push_back(node);
        }
    }

    // Add dependencies: each layer depends on the previous
    for (int l = 1; l < layers; ++l) {
        for (int w = 0; w < width; ++w) {
            // Simple pattern: every node depends on all nodes in previous layer
            for (auto* prev : nodes[l - 1]) {
                graph.addDependency(prev, nodes[l][w]);
            }
        }
    }

    // Execute DAG
    auto start = std::chrono::high_resolution_clock::now();
    scheduler.execute(graph);

    pool.stop();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Execution finished in " << elapsed.count() << " seconds\n";
}
