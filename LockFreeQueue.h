#pragma once
#include <atomic>
#include <vector>
#include <stdexcept>
#include <memory>
#include <cmath>

template<typename T>
class LockFreeQueue {
private:
    std::atomic<size_t> head;    // Consumer reads from head
    std::atomic<size_t> tail;    // Producer writes at tail
    size_t capacity;             // Current capacity of the buffer
    std::unique_ptr<std::vector<T>> buffer; // Dynamic buffer

public:
    explicit LockFreeQueue(size_t initial_capacity = 16)
        : head(0), tail(0), capacity(initial_capacity) {
        if (initial_capacity == 0 || (initial_capacity & (initial_capacity - 1)) != 0) {
            throw std::runtime_error("Initial capacity must be a power of 2 for masking");
        }
        buffer = std::make_unique<std::vector<T>>(initial_capacity);
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    LockFreeQueue(LockFreeQueue&& other) noexcept
        : head(other.head.load()), tail(other.tail.load()),
          capacity(other.capacity), buffer(std::move(other.buffer)) {}

    LockFreeQueue& operator=(LockFreeQueue&& other) noexcept {
        if (this != &other) {
            head.store(other.head.load());
            tail.store(other.tail.load());
            capacity = other.capacity;
            buffer = std::move(other.buffer);
        }
        return *this;
    }

    bool enqueue(const T& item) {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t h = head.load(std::memory_order_acquire);

        if (((t + 1) & (capacity - 1)) == h) {
            // Queue is full, so we need to resize
            resize();
            t = tail.load(std::memory_order_relaxed);  // Re-check tail after resize
        }

        (*buffer)[t] = item;
        tail.store((t + 1) & (capacity - 1), std::memory_order_release);
        return true;
    }

    bool dequeue(T& item) {
        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);

        if (h == t) {
            return false; // Queue is empty
        }

        item = (*buffer)[h];  // Assign buffer element to item
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

private:
    void resize() {
        // Calculate new capacity
        size_t new_capacity = ceil(capacity*1.5);

        // Create a new buffer with double the size
        auto new_buffer = std::make_unique<std::vector<T>>(new_capacity);

        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);

        // Copy elements from old buffer to new buffer
        size_t size = (t - h) & (capacity - 1);  // Number of elements in the queue
        for (size_t i = 0; i < size; ++i) {
            (*new_buffer)[i] = (*buffer)[(h + i) & (capacity - 1)];
        }

        // Update head and tail for new buffer
        head.store(0, std::memory_order_release);
        tail.store(size, std::memory_order_release);

        // Swap the old buffer with the new buffer
        buffer = std::move(new_buffer);
        capacity = new_capacity;
    }
};
