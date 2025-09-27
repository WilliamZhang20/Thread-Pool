#pragma once
#include <atomic>
#include <vector>
#include <stdexcept>

template<typename T>
class LockFreeQueue {
private:
    std::vector<T> buffer;
    size_t capacity;

    alignas(64) std::atomic<size_t> head; // consumer reads from head
    alignas(64) std::atomic<size_t> tail; // producer writes at tail

public:
    explicit LockFreeQueue(size_t cap)
        : buffer(cap), capacity(cap), head(0), tail(0) {
        if ((cap & (cap - 1)) != 0) {
            throw std::runtime_error("Capacity must be power of 2 for masking");
        }
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    LockFreeQueue(LockFreeQueue&& other) noexcept
        : buffer(std::move(other.buffer)),
        capacity(other.capacity),
        head(other.head.load()),
        tail(other.tail.load()) {}

    LockFreeQueue& operator=(LockFreeQueue&& other) noexcept {
        if (this != &other) {
            buffer = std::move(other.buffer);
            capacity = other.capacity;
            head.store(other.head.load());
            tail.store(other.tail.load());
        }
        return *this;
    }

    bool enqueue(const T& item) {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t h = head.load(std::memory_order_acquire);

        if (((t + 1) & (capacity - 1)) == h) {
            return false; // full
        }

        buffer[t] = item;
        tail.store((t + 1) & (capacity - 1), std::memory_order_release);
        return true;
    }

    bool dequeue(T& item) {
        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);

        if (h == t) {
            return false; // empty
        }

        item = buffer[h]; // assign buffer element to
        head.store((h + 1) & (capacity - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

    bool full() const {
        size_t next_tail = (tail.load(std::memory_order_relaxed) + 1) & (capacity - 1);
        return next_tail == head.load(std::memory_order_acquire);
    }
};
