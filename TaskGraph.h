#include <functional>
#include <atomic>
#include <vector>
#include <memory>

struct TaskNode {
    using Task = std::function<void()>;

    Task work;
    std::atomic<size_t> remainingDeps{0};
    std::vector<TaskNode*> dependents;

    explicit TaskNode(Task t) : work(std::move(t)) {}
};

class TaskGraph {
public:
    using Task = std::function<void()>;

    TaskNode* addTask(Task t) {
        nodes.push_back(std::make_unique<TaskNode>(std::move(t)));
        return nodes.back().get();
    }

    void addDependency(TaskNode* before, TaskNode* after) {
        after->remainingDeps++;
        before->dependents.push_back(after);
    }

private:
    std::vector<std::unique_ptr<TaskNode>> nodes;
    friend class TaskScheduler;  // Scheduler needs access
};
