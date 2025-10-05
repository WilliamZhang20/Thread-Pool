#include "../TaskScheduler.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <iostream>

double heavyComputation(int n) {
    double sum = 0.0;
    for (int i = 1; i < n; ++i) {
        sum += std::sin(i) * std::cos(i / 2.0);
    }
    return sum;
}

// Helper: execute graph sequentially ignoring dependencies (for comparison)
void executeSequential(const std::vector<std::vector<TaskNode*>>& nodes) {
    for (auto& layer : nodes) {
        for (auto* node : layer) {
            node->work();  // run task
        }
    }
}

int main() {
    constexpr int layers = 10;       // depth of DAG
    constexpr int width  = 8000;     // number of tasks per layer

    // Build the DAG
    TaskGraph graph;
    std::vector<std::vector<TaskNode*>> nodes(layers);

    for (int l = 0; l < layers; ++l) {
        for (int w = 0; w < width; ++w) {
            auto* node = graph.addTask([l, w]() {
                heavyComputation(50000 + (l * 1000 + w * 100));
            });
            nodes[l].push_back(node);
        }
    }

    for (int l = 1; l < layers; ++l) {
        for (int w = 0; w < width; ++w) {
            int prevIndex = w % width;               // simple mapping
            graph.addDependency(nodes[l-1][prevIndex], nodes[l][w]);
        }
    }

    // Sequential execution
    auto seqStart = std::chrono::high_resolution_clock::now();
    executeSequential(nodes);
    auto seqEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> seqElapsed = seqEnd - seqStart;
    std::cout << "Sequential execution finished in " << seqElapsed.count() << " seconds\n";

    // Parallel execution
    ThreadPool pool(4);              // 4 worker threads
    TaskScheduler scheduler(pool);

    auto parStart = std::chrono::high_resolution_clock::now();
    scheduler.execute(graph);
    pool.stop();
    auto parEnd = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> parElapsed = parEnd - parStart;
    std::cout << "Parallel execution finished in " << parElapsed.count() << " seconds\n";

    // Compute speedup
    double speedup = seqElapsed.count() / parElapsed.count();
    std::cout << "Speedup: " << speedup << "×\n";
}
