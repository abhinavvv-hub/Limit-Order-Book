#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

/*
=================================================================================
    Lock‑free, pre‑allocated object pool using a free‑list of indices.
    No 'new' or 'delete' is ever called. All memory is reserved at compile time.
=================================================================================
*/
template <typename T, size_t Size>
class ObjectPool {
public:
    ObjectPool() noexcept {
        /* Build a free list -> freeStack[i] points to the next free slot. */
        for (size_t i = 0; i < Size; i += 1)
            freeStack[i] = static_cast<int>(i + 1);
        freeStack[Size - 1] = -1;         /* end of free list */
        freeHead = 0;
    }

    [[nodiscard]] int32_t allocate() noexcept { /* Returns a free index from the pool, or -1 if exhausted. */
        if (freeHead == -1) [[unlikely]]
            return -1;                    /* pool exhausted */
        int32_t idx = freeHead;
        freeHead = freeStack[idx];        /* pop from free list */
        return idx;
    }

    void deallocate(int32_t idx) noexcept { /* Returns an index to the free list. */
        freeStack[idx] = freeHead;
        freeHead = idx;
    }

    /* Access the underlying object by index. */
    T& operator[](int32_t idx) noexcept { return pool[idx]; }
    const T& operator[](int32_t idx) const noexcept { return pool[idx]; }

private:
    std::array<T, Size>         pool;         /* actual objects */
    std::array<int32_t, Size>   freeStack;    /* reused as next-pointer array */
    int32_t                     freeHead;     /* head of the free list */
};
