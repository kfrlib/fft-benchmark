/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */
#include "benchmark.hpp"
#include <atomic>
#include <vector>

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>

#endif
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace bm
{

void use_from_outside(const char volatile* in) { (void)in; }

static size_t page_size()
{
#if defined(_WIN32)
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return static_cast<size_t>(si.dwPageSize);
#else
    long ps = ::sysconf(_SC_PAGESIZE);
    if (ps <= 0)
    {
        fprintf(stderr, "sysconf(_SC_PAGESIZE) failed\n");
        std::abort();
    }
    return static_cast<size_t>(ps);
#endif
}

static inline size_t round_up_to_page(size_t size)
{
    const size_t ps = page_size();
    return (size + ps - 1) & ~(ps - 1);
}

void* page_aligned_alloc(size_t size)
{
    if (size == 0)
        return nullptr;
    const size_t aligned = round_up_to_page(size);

#if defined(_WIN32)
    void* p = ::VirtualAlloc(nullptr, aligned, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    ::VirtualLock(p, aligned); // prevent paging to disk, which would cause huge latency spikes

    if (!p)
    {
        fprintf(stderr, "VirtualAlloc failed\n");
        std::abort();
    }

    return p;
#else
    void* p = ::mmap(nullptr, aligned, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (p == MAP_FAILED)
    {
        fprintf(stderr, "mmap failed\n");
        std::abort();
    }

    return p;
#endif
}

void page_aligned_free(void* ptr, size_t size)
{
    if (!ptr)
        return;

    const std::size_t aligned = round_up_to_page(size);

#if defined(_WIN32)
    (void)aligned;
    ::VirtualUnlock(ptr, aligned);
    ::VirtualFree(ptr, 0, MEM_RELEASE);
#else
    ::munmap(ptr, aligned);
#endif
}

namespace details
{

#if defined(_WIN32)

static HANDLE g_thread = nullptr;
static std::atomic<bool> g_run{ false };

static DWORD find_sibling(DWORD cpu)
{
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);

    std::vector<char> buf(len);
    auto* info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf.data();

    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &len))
        return cpu;

    char* ptr = buf.data();
    char* end = ptr + len;

    while (ptr < end)
    {
        auto* e = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;

        if (e->Relationship == RelationProcessorCore)
        {
            KAFFINITY mask = e->Processor.GroupMask[0].Mask;

            if (mask & (1ull << cpu))
            {
                for (DWORD i = 0; i < 64; ++i)
                {
                    if ((mask & (1ull << i)) && i != cpu)
                        return i;
                }
            }
        }

        ptr += e->Size;
    }

    return cpu; // no sibling (no SMT)
}

static DWORD WINAPI spin(void*)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (g_run.load(std::memory_order_relaxed))
    {
        for (uint32_t i = 0; i < 100000; ++i)
            _mm_pause();
    }
    return 0;
}

void run_spin_thread_on_ht_sibling(DWORD core)
{
    DWORD sib = find_sibling(core);
    if (sib == core)
        return; // no SMT

    g_run    = true;
    g_thread = CreateThread(nullptr, 0, spin, nullptr, 0, nullptr);
    SetThreadAffinityMask(g_thread, 1ull << sib);
}

void terminate_spin_thread()
{
    if (!g_thread)
        return;

    g_run = false;
    WaitForSingleObject(g_thread, INFINITE);
    CloseHandle(g_thread);
    g_thread = nullptr;
}

#endif

uint64_t start_time;

double tsc_scale = 0;

void calibrate_tsc()
{
    if (tsc_scale != 0)
        return;

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
static void (*NtSetTimerResolution)(ULONG, bool, PULONG) = nullptr;
static ULONG oldresolution;
#endif
#ifdef __linux__
static cpu_set_t old_cpuset;
#endif
#ifdef __APPLE__
static qos_class_t old_qos_class;
static int old_qos_relative_priority;
#endif

static int ideal_core = 2;
void run_on_core(int core) { ideal_core = core; }

benchmark_scope::benchmark_scope()
{
#ifdef _WIN32
    HANDLE thrd = GetCurrentThread();
    old_prio    = GetThreadPriority(thrd);
    // printf("Setting realtime priority and affinity to core %d\n", ideal_core);
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    SetThreadPriority(thrd, THREAD_PRIORITY_HIGHEST);
    old_affmask = SetThreadAffinityMask(thrd, 1ull << ideal_core);

#if 0
    printf("Creating spin thread on SMT sibling of core %d to keep it busy\n", ideal_core);
    details::run_spin_thread_on_ht_sibling(ideal_core);
#endif

    // printf("Setting timer resolution to 0.5ms\n");
    NtSetTimerResolution =
        (void (*)(ULONG, bool, PULONG))GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetTimerResolution");
    if (NtSetTimerResolution)
    {
        NtSetTimerResolution(5000, true, &oldresolution);
    }
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

    details::terminate_spin_thread();
    if (NtSetTimerResolution)
    {
        NtSetTimerResolution(oldresolution, false, &oldresolution);
    }
#endif
#ifdef __linux__
    sched_setaffinity(0, sizeof(old_cpuset), &old_cpuset);
#endif
#ifdef __APPLE__
    pthread_set_qos_class_self_np(old_qos_class, old_qos_relative_priority);
#endif
}

} // namespace bm