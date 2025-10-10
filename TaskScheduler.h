#pragma once
#include <functional>
#include <atomic>
#include <vector>
#include <memory>
#include <future>
#include <mutex>
#include "ThreadPool.h"

class TaskGraphScheduler {
public:
    struct ITaskNode {
        std::vector<ITaskNode*> dependents;
        std::atomic<size_t> remainingDeps{0};
        virtual void run() = 0;
        virtual ~ITaskNode() = default;
    };

    template<typename T>
    struct TaskNode : ITaskNode {
        std::packaged_task<T()> task;
        std::future<T> fut;

        explicit TaskNode(std::function<T()> t)
            : task(std::move(t)), fut(task.get_future()) {}

        void run() override { task(); }
    };

    explicit TaskGraphScheduler(ThreadPool& pool) : pool(pool) {}

    // TaskHandle wrapper
    template<typename T>
    struct TaskHandle {
        std::shared_ptr<TaskNode<T>> node;

        explicit TaskHandle(std::shared_ptr<TaskNode<T>> n) : node(std::move(n)) {}
        std::future<T>& get_future() { return node->fut; }
    };

    // Submit independent task
    template<typename T>
    TaskHandle<T> submit(std::function<T()> func) {
        auto node = std::make_shared<TaskNode<T>>(std::move(func));
        nodes.push_back(node);
        submitIfReady(node.get());
        return TaskHandle<T>(node);
    }

    // Submit task with dependencies
    template<typename T>
    TaskHandle<T> submit_with_deps(std::function<T()> func, const std::vector<std::shared_ptr<ITaskNode>>& deps) {
        auto node = std::make_shared<TaskNode<T>>(std::move(func));

        node->remainingDeps.store(deps.size(), std::memory_order_relaxed);
        for (auto& dep : deps) {
            dep->dependents.push_back(node.get());
        }

        nodes.push_back(node);
        submitIfReady(node.get());
        return TaskHandle<T>(node);
    }

private:
    ThreadPool& pool;
    std::vector<std::shared_ptr<ITaskNode>> nodes;
    
    void submitIfReady(ITaskNode* node) {
        if (node->remainingDeps.load(std::memory_order_acquire) == 0) {
            submitNode(node);
        }
    }

    void submitNode(ITaskNode* node) {
        pool.submit([this, node]() {
            node->run();

            // Notify dependents
            for (auto* dep : node->dependents) {
                if (dep->remainingDeps.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    submitNode(dep);
                }
            }
        });
    }
};