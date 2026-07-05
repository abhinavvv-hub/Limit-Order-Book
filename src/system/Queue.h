#pragma once

#include <atomic>
#include <array>
#include <cstddef>

/*
=============================================================================
Single‑Producer, Single‑Consumer (SPSC) lock‑free ring buffer.
Uses cache‑line padding to eliminate false sharing between the producer
and consumer. Suitable for low‑latency message passing between threads.
=============================================================================
*/

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    SPSCQueue() : writeIdx(0), readIdx(0) {}

    /* Non‑copyable and non‑movable (atomic members) */
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    /*
    Try to push one element. Returns false if the queue is full.
    Called only by the producer thread.
    */
    [[nodiscard]] bool push(const T& item) noexcept {
        const size_t w = writeIdx.load(std::memory_order_relaxed);
        const size_t next = (w + 1) & mask;

        /* Queue full if next write would overlap with read index. */
        if (next == readIdx.load(std::memory_order_acquire)) [[unlikely]]
            return false;

        buffer[w] = item;
        writeIdx.store(next, std::memory_order_release);
        return true;
    }

    /*
    Try to pop one element. Returns false if the queue is empty.
    Called only by the consumer thread.
    */
    [[nodiscard]] bool pop(T& item) noexcept {
        const size_t r = readIdx.load(std::memory_order_relaxed);

        /* Queue empty if read index catches up to write index. */
        if (r == writeIdx.load(std::memory_order_acquire)) [[unlikely]]
            return false;

        item = buffer[r];
        readIdx.store((r + 1) & mask, std::memory_order_release);
        return true;
    }

private:
    static constexpr size_t mask = Capacity - 1;
    std::array<T, Capacity> buffer;
    
    /*
    Separate cache lines to avoid false sharing.
    Producer writes to writeIdx, consumer reads it;
    consumer writes to readIdx, producer reads it.
    */
    alignas(64) std::atomic<size_t> writeIdx;
    alignas(64) std::atomic<size_t> readIdx;
};
