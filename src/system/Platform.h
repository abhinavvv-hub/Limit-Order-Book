#pragma once

#include <windows.h>
#include <intrin.h>
#include <cstdint>

/*
=============================================================================
Platform‑specific low‑latency utilities:
  - CPU core pinning
  - High‑resolution timestamp (RDTSC)
=============================================================================
*/

/* Pin the calling thread to a specific logical CPU core. */
inline void pin_thread_to_core(int core_id) {
    DWORD_PTR mask = 1ULL << core_id;
    SetThreadAffinityMask(GetCurrentThread(), mask);
}

/* Read the Time‑Stamp Counter (TSC) – a fast, wall‑clock independent cycle counter. */
inline uint64_t rdtsc() {
    return __rdtsc();
}

/*
Busy‑wait (spin) until the TSC reaches 'target'.
_mm_pause() hints the CPU that we are in a spin‑loop, saving power and
reducing contention on the memory bus.
*/
inline void busy_wait_until(uint64_t target) {
    while (rdtsc() < target)
        _mm_pause();
}
