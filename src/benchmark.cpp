/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */
#include "benchmark.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif
#ifdef __APPLE__
#include <pthread.h>
#endif

void use_from_outside(const char volatile* in) { (void)in; }

namespace details
{

std::chrono::nanoseconds start_time;

double tsc_scale = 0;

void calibrate_tsc()
{
#if defined(__aarch64__)
    // On AArch64 the virtual counter CNTVCT_EL0 is driven by a fixed-frequency
    // oscillator whose rate is published in CNTFRQ_EL0. No measurement loop is
    // needed — just read the register directly.
    uint64_t cntfrq;
    asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(cntfrq));
    tsc_scale = 1e9 / static_cast<double>(cntfrq); // ns per tick
#else
    // On x86, busy-wait against os_time() so the CPU stays at its full running
    // frequency throughout calibration (sleep_for causes a frequency drop on
    // wakeup that corrupts the TSC-to-wall-clock ratio).
    constexpr int count     = 10;
    constexpr auto interval = std::chrono::milliseconds(50);
    double tsc_freq         = 0;
    for (int i = 0; i < count; ++i)
    {
        // Align to a fresh os_time() tick to avoid a partial first interval.
        std::chrono::nanoseconds os_start = os_time();
        while (os_time() == os_start)
            ;
        os_start           = os_time();
        uint64_t tsc_start = rdtsc();
        std::chrono::nanoseconds os_end;
        do
        {
            os_end = os_time();
        } while (os_end - os_start < interval);
        uint64_t tsc_duration                = rdtsc() - tsc_start;
        std::chrono::nanoseconds os_duration = os_end - os_start;
        tsc_freq += tsc_duration / static_cast<double>(os_duration.count());
    }
    tsc_freq /= count; // ticks/ns
    tsc_scale = 1.0 / tsc_freq;
#endif
}

#ifdef _WIN32
double get_qpc_scale()
{
    LARGE_INTEGER lpFrequency;
    QueryPerformanceFrequency(&lpFrequency);
    return 1'000'000'000.0 / lpFrequency.QuadPart;
}
uint64_t get_qpc_current()
{
    LARGE_INTEGER lpPerformanceCount;
    QueryPerformanceCounter(&lpPerformanceCount);
    return lpPerformanceCount.QuadPart;
}

double qpc_scale = details::get_qpc_scale();
#endif
} // namespace details

#ifdef _WIN32
static int old_prio;
static DWORD_PTR old_affmask;
#endif
#ifdef __linux__
static cpu_set_t old_cpuset;
#endif
#ifdef __APPLE__
static qos_class_t old_qos_class;
static int old_qos_relative_priority;
#endif

static int ideal_core = 0;
void run_on_core(int core) { ideal_core = core; }

benchmark_scope::benchmark_scope()
{
#ifdef _WIN32
    HANDLE thrd = GetCurrentThread();
    old_prio    = GetThreadPriority(thrd);
    SetThreadPriority(thrd, THREAD_PRIORITY_HIGHEST);
    old_affmask = SetThreadAffinityMask(thrd, 1ull << ideal_core);
#endif
#ifdef __linux__
    sched_getaffinity(0, sizeof(old_cpuset), &old_cpuset);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(ideal_core, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
#endif
#ifdef __APPLE__
    // CPU affinity is not user-controllable on macOS/Apple Silicon.
    // Boost thread QoS to USER_INTERACTIVE to reduce scheduling preemptions.
    pthread_get_qos_class_np(pthread_self(), &old_qos_class, &old_qos_relative_priority);
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}
benchmark_scope::~benchmark_scope()
{
#ifdef _WIN32
    HANDLE thrd = GetCurrentThread();
    DisableThreadProfiling(thrd);
    SetThreadPriority(thrd, old_prio);
    SetThreadAffinityMask(thrd, old_affmask);
#endif
#ifdef __linux__
    sched_setaffinity(0, sizeof(old_cpuset), &old_cpuset);
#endif
#ifdef __APPLE__
    pthread_set_qos_class_self_np(old_qos_class, old_qos_relative_priority);
#endif
}
