#pragma once

#include "prelude.hpp"

#if defined(_WIN32)
#include <processthreadsapi.h>
#elif defined(__linux__)
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace exprbench {

inline bool apply_high_priority_best_effort() {
#if defined(_WIN32)
    return SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS) != 0;
#elif defined(__linux__)
    // Non-root may fail for negative nice values; use best effort.
    return setpriority(PRIO_PROCESS, 0, -10) == 0;
#else
    return false;
#endif
}

inline bool apply_cpu_pin_best_effort(int threads) {
    const int use_threads = std::max(1, threads);
#if defined(_WIN32)
    if (use_threads >= static_cast<int>(sizeof(DWORD_PTR) * 8)) return false;
    DWORD_PTR mask = 0;
    for (int i = 0; i < use_threads; ++i) {
        mask |= (static_cast<DWORD_PTR>(1) << i);
    }
    return SetProcessAffinityMask(GetCurrentProcess(), mask) != 0;
#elif defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    int limit = std::max(1, std::min(use_threads, static_cast<int>(std::max<long>(1, ncpu))));
    for (int i = 0; i < limit; ++i) CPU_SET(i, &set);
    return sched_setaffinity(0, sizeof(set), &set) == 0;
#else
    (void)use_threads;
    return false;
#endif
}

} // namespace exprbench

