#include <atomic>
#include "ThreadPool.h"
#include "TaskGraph.h"

class TaskScheduler {
public:
    explicit TaskScheduler(ThreadPool& pool) : pool(pool) {}

    void execute(TaskGraph& graph) {
        // Submit all tasks with no dependencies
        for (auto& node : graph.nodes) {
            if (node->remainingDeps.load(std::memory_order_relaxed) == 0) {
                submitNode(node.get());
            }
        }
    }

private:
    ThreadPool& pool;

    void submitNode(TaskNode* node) {
        pool.submit([this, node]() {
            node->work();  // Run the task

            // Notify dependents
            for (auto* dep : node->dependents) {
                if (--dep->remainingDeps == 0) {
                    submitNode(dep);
                }
            }
        });
    }
};
