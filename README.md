# Ultra-Low Latency Limit Order Book (LOB)

A high-performance, deterministic matching engine written in modern **C++20** and optimized exclusively for **Windows 11** using the native Win32 API and the **MinGW-w64 GCC 16.1.0** toolchain. 

This project demonstrates core low-latency software engineering principles used in High-Frequency Trading (HFT) environments, aiming for sub-microsecond deterministic execution paths with **zero heap allocations** on the critical hot path.

---

## Key Features & Low-Latency Architecture

* **Zero-Allocation Memory Pool:** Avoids the overhead and non-deterministic behavior of OS heap allocations (`new`/`delete`). Uses a pre-allocated static cache (`ObjectPool`) with a fast-recycling free list for structural memory management on the hot path.
* **Intrusive Doubly-Linked List:** Price levels manage orders using an intrusive doubly-linked list layout. By embedding list pointers directly inside the `Order` struct, cache thrashing is heavily minimized, granting strict $O(1)$ operations for insertions, execution updates, and cancellations.
* **Lock-Free SPSC Queue:** Isolates network I/O from the execution pathway. A Single-Producer, Single-Consumer ring buffer passes incoming parsed UDP frames straight to the engine thread without mutex locks.
* **Cache-Line Alignment (`alignas(64)`):** Core shared variables and ring buffer indices are explicitly aligned to 64-byte boundaries to eliminate performance degradation from false sharing across CPU cores.
* **Hardware-Level Optimization:** Employs advanced compiler branch prediction optimizations via `[[likely]]` and `[[unlikely]]` attributes to prioritize the successful execution paths directly within CPU instruction pipelines.
* **Win32 Platform Pinning & Timing:**
  * Bypasses the default OS thread scheduler via `SetThreadAffinityMask` to isolate and pin the matching engine thread to a specific CPU core, keeping L1/L2 caches permanently warm.
  * Captures precise nanosecond metrics directly from CPU cycles using hardware time-stamp counters (`__rdtsc()`).

---

## 🛠️ Tech Stack & Toolchain

* **Operating System:** Windows 11 (Native Win32 Architecture)
* **Compiler:** MinGW-w64 GCC 16.1.0 (`g++`)
* **Build System:** CMake + Ninja Build
* **Language Standard:** C++20
* **Network Protocol:** UDP Multicast via Winsock2 (`ws2_32`)

---

## 📂 Project Structure

```text
limit_order_book/
├── CMakeLists.txt             # Optimized build configuration (O3, march=native, flto)
├── src/
│   ├── main.cpp               # System orchestration & core lifecycle loop
│   ├── engine/
│   │   ├── OrderBook.h        # Matching logic and array-based order tracking
│   │   └── Types.h            # Data structures, enums, and intrusive layout pointers
│   ├── memory/
│   │   └── ObjectPool.h       # High-performance static memory arena
│   ├── network/
│   │   └── UdpReceiver.h      # Winsock2 non-blocking UDP multicast listener
│   └── system/
│       ├── Queue.h            # SPSC lock-free atomic circular queue
│       └── Platform.h         # Win32 hardware thread-affinity & RDTSC timers
└── .github/
    └── workflows/
        └── build_and_test.yml # CI automation executing native MinGW toolchain builds
