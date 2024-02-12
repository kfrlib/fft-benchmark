/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2023 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */
#include "benchmark.hpp"
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace details
{

std::chrono::nanoseconds start_time;

double tsc_scale = 0;

void calibrate_tsc()
{
    constexpr int count = 10;
    double tsc_freq     = 0;
    for (int i = 0; i < count; ++i)
    {
        std::chrono::nanoseconds os_start = os_time();
        uint64_t tsc_start                = rdtsc();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uint64_t tsc_duration                = rdtsc() - tsc_start;
        std::chrono::nanoseconds os_duration = os_time() - os_start;
        tsc_freq += tsc_duration / static_cast<double>(os_duration.count());
    }
    tsc_freq /= count; // ticks/ns
    tsc_scale = 1.0 / tsc_freq;
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
}
