#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>
#include <cassert>
#include <utility>
// Based on the classic Chase-Lev work-stealing deque design.
// Notes:
//  - Only one thread (the "owner") should call push_bottom/pop_bottom.
//  - Many other threads may concurrently call steal_top.
//  - The implementation grows the circular buffer when needed; growth is performed
//    by the owner only (so no expensive synchronization needed for resizing).

template <typename T>
class LockFreeDeque {
private:
    struct buffer {
        size_t capacity;  // power of two
        std::aligned_storage_t<sizeof(T), alignof(T)> *data;  // Aligned raw storage
        buffer(size_t cap)
            : capacity(cap), data(static_cast<std::aligned_storage_t<sizeof(T), alignof(T)>*>(operator new[](sizeof(std::aligned_storage_t<sizeof(T), alignof(T)>) * cap))) {}

        ~buffer() {
            operator delete[](data);
        }
    };

    std::atomic<size_t> top;
    std::atomic<size_t> bottom;

    // Buffer pointer is only updated by owner; thieves only read it.
    std::atomic<buffer*> buf;

    static size_t next_pow2(size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    // Owner-only: grow the buffer when it's full
    void grow(buffer *old) {
        size_t oldcap = old->capacity;
        size_t newcap = oldcap << 1;
        buffer *b = new buffer(newcap);

        // Copy current valid elements [top, bottom)
        size_t t = top.load(std::memory_order_acquire);
        size_t btm = bottom.load(std::memory_order_relaxed);
        size_t sz = btm - t;
        for (size_t i = 0; i < sz; ++i) {
            // Use placement new to copy
            new (&b->data[i]) T(std::move(reinterpret_cast<T&>(old->data[( (t + i) & (oldcap - 1) )]))); 
        }
        // Publish new buffer
        buf.store(b, std::memory_order_release);

        // Adjust indices: set top = 0, bottom = sz to match copied layout
        top.store(0, std::memory_order_release);
        bottom.store(sz, std::memory_order_release);

        // Delete old buffer
        delete old;
    }

public:
    LockFreeDeque(size_t initial_capacity = 1024) {
        size_t c = next_pow2(std::max<size_t>(2, initial_capacity));
        buffer *b = new buffer(c);
        buf.store(b, std::memory_order_release);
        top.store(0, std::memory_order_relaxed);
        bottom.store(0, std::memory_order_relaxed);
    }

    ~LockFreeDeque() {
        buffer *b = buf.load(std::memory_order_relaxed);
        if (!b) return;
        size_t t = top.load(std::memory_order_relaxed);
        size_t btm = bottom.load(std::memory_order_relaxed);
        size_t cap = b->capacity;
        if (btm > t) {
            for (size_t i = t; i < btm; ++i) {
                size_t idx = i & (cap - 1);
                std::launder(reinterpret_cast<T*>(&b->data[idx]))->~T();
            }
        }
        delete b;
    }

    LockFreeDeque(const LockFreeDeque&) = delete;
    LockFreeDeque& operator=(const LockFreeDeque&) = delete;

    LockFreeDeque(LockFreeDeque&& other) noexcept {
        top.store(other.top.load(std::memory_order_relaxed), std::memory_order_relaxed);
        bottom.store(other.bottom.load(std::memory_order_relaxed), std::memory_order_relaxed);
        buf.store(other.buf.load(std::memory_order_relaxed), std::memory_order_relaxed);
        other.buf.store(nullptr, std::memory_order_relaxed);
    }

    LockFreeDeque& operator=(LockFreeDeque&& other) noexcept {
        if (this != &other) {
            // Free current buffer if any
            auto old = buf.load(std::memory_order_relaxed);
            delete old;

            top.store(other.top.load(std::memory_order_relaxed), std::memory_order_relaxed);
            bottom.store(other.bottom.load(std::memory_order_relaxed), std::memory_order_relaxed);
            buf.store(other.buf.load(std::memory_order_relaxed), std::memory_order_relaxed);
            other.buf.store(nullptr, std::memory_order_relaxed);
        }
        return *this;
    }

    void push_bottom(T item) {
        size_t btm = bottom.load(std::memory_order_relaxed);
        size_t t = top.load(std::memory_order_acquire);
        buffer *cbuf = buf.load(std::memory_order_acquire);
        size_t cap = cbuf->capacity;

        if (btm - t >= cap - 1) {
            grow(cbuf);
            cbuf = buf.load(std::memory_order_acquire);
            cap = cbuf->capacity;
            btm = bottom.load(std::memory_order_relaxed);
        }

        size_t idx = btm & (cap - 1);
        // Use placement new to construct the object
        new (&cbuf->data[idx]) T(std::move(item));
        bottom.store(btm + 1, std::memory_order_release);
    }

    // Owner thread: pop from bottom. Returns std::nullopt when empty.
    std::optional<T> pop_bottom() {
        size_t b = bottom.load(std::memory_order_relaxed);
        size_t t = top.load(std::memory_order_acquire);
        if (b <= t) {
            return std::nullopt;
        }

        size_t new_b = b - 1;
        bottom.store(new_b, std::memory_order_release);

        t = top.load(std::memory_order_acquire);  // Reload after store
        if (new_b < t) {
            bottom.store(b, std::memory_order_relaxed);  // Restore
            return std::nullopt;
        }

        buffer *cbuf = buf.load(std::memory_order_acquire);
        size_t cap = cbuf->capacity;
        size_t idx = new_b & (cap - 1);
        T* ptr = reinterpret_cast<T*>(&cbuf->data[idx]);
        T result = std::move(*ptr);
        ptr->~T();

        return std::make_optional<T>(std::move(result));
    }

    // Thief thread(s): steal from top. Returns std::nullopt if empty / failed.
    std::optional<T> steal_top() {
        size_t t = top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t btm = bottom.load(std::memory_order_acquire);

        if (t >= btm) {
            // Empty
            return std::nullopt;
        }

        buffer *cbuf = buf.load(std::memory_order_acquire);
        size_t cap = cbuf->capacity;
        size_t idx = t & (cap - 1);

        T *ptr = reinterpret_cast<T*>(&cbuf->data[idx]);
        T result = std::move(*ptr);

        // Attempt to CAS top forward
        size_t expected = t;
        if (top.compare_exchange_strong(expected, t + 1,
                                        std::memory_order_seq_cst,
                                        std::memory_order_relaxed)) {
            // Success: we logically stole the element; destroy in place and return
            ptr->~T();
            return std::make_optional<T>(std::move(result));
        } else {
            // Someone else (another thief or owner) raced; failed to steal
            *ptr = std::move(result);
            return std::nullopt;
        }
    }

    // Approximate size
    size_t size_approx() const {
        size_t t = top.load(std::memory_order_acquire);
        size_t btm = bottom.load(std::memory_order_acquire);
        return (btm >= t) ? (btm - t) : 0;
    }
};