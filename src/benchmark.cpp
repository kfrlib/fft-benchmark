/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
#endif
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <pthread.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "benchmark.hpp"
#include <atomic>
#include <vector>

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
    long ps = sysconf(_SC_PAGESIZE);
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
    void* p = mmap(nullptr, aligned, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

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
    munmap(ptr, aligned);
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

#if defined(USE_OS_TIME)
std::chrono::nanoseconds start_time;
#else
uint64_t start_time;
#endif

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
int get_ideal_core() { return ideal_core; }

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
static unsigned int old_mxcsr              = 0;
static constexpr unsigned int FTZ_DAZ_BITS = 0x8040; // FTZ (bit 15) | DAZ (bit 6)
#elif defined(__aarch64__) || defined(__ARM_NEON)
static uint64_t old_fpcr = 0;
// AH (bit 1) flushes denormal inputs to zero; FZ (bit 24) flushes denormal
// outputs to zero. Both are AArch64 NEON/VFP control bits.
static constexpr uint64_t FZ_BITS = (1u << 24) | (1u << 1);
#endif

void enable_denormals_flush()
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    old_mxcsr = _mm_getcsr();
    _mm_setcsr((old_mxcsr & ~FTZ_DAZ_BITS) | FTZ_DAZ_BITS);
#elif defined(__aarch64__)
    uint64_t fpcr;
    asm volatile("mrs %0, FPCR" : "=r"(fpcr));
    old_fpcr = fpcr;
    asm volatile("msr FPCR, %0" : : "r"((fpcr & ~FZ_BITS) | FZ_BITS));
#elif defined(__ARM_NEON) && defined(__arm__)
    uint32_t fpscr;
    asm volatile("vmrs %0, fpscr" : "=r"(fpscr));
    old_fpcr = fpscr;
    // On AArch32 NEON, FZ is bit 24 of the FPSCR.
    asm volatile("vmsr fpscr, %0" : : "r"((fpscr & ~(1u << 24)) | (1u << 24)));
#endif
}

void restore_denormals_flush()
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    _mm_setcsr(old_mxcsr);
#elif defined(__aarch64__)
    asm volatile("msr FPCR, %0" : : "r"(old_fpcr));
#elif defined(__ARM_NEON) && defined(__arm__)
    asm volatile("vmsr fpscr, %0" : : "r"(static_cast<uint32_t>(old_fpcr)));
#endif
}

benchmark_scope::benchmark_scope()
{
#ifdef _WIN32
    HANDLE thrd = GetCurrentThread();
    old_prio    = GetThreadPriority(thrd);
    // printf("Setting realtime priority and affinity to core %d\n", ideal_core);
    if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS) ||
        !SetThreadPriority(thrd, THREAD_PRIORITY_TIME_CRITICAL))
    {
        fprintf(stderr, "Failed to set realtime priority\n");
        if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS) ||
            !SetThreadPriority(thrd, THREAD_PRIORITY_HIGHEST))
        {
            fprintf(stderr, "Failed to set high priority\n");
            std::abort();
        }
    }
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
    enable_denormals_flush();
}
benchmark_scope::~benchmark_scope()
{
    restore_denormals_flush();
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

std::string cpu_name_from_os()
{
#if defined(_WIN32)
    // On Windows the registry is the authoritative source for the model name
    // (matches what is shown in System settings / Task Manager). CPUID brand
    // strings are absent or generic on some virtualized / emulated setups.
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ,
                      &hKey) == ERROR_SUCCESS)
    {
        wchar_t wide[256] = {};
        DWORD size        = sizeof(wide);
        DWORD type        = 0;
        if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, &type, reinterpret_cast<LPBYTE>(wide),
                             &size) == ERROR_SUCCESS &&
            type == REG_SZ)
        {
            RegCloseKey(hKey);
            int len = static_cast<int>(wcslen(wide));
            // Worst case: 4 UTF-8 bytes per wchar + NUL.
            std::string out(len * 4 + 1, '\0');
            int n = WideCharToMultiByte(CP_UTF8, 0, wide, len, out.data(), static_cast<int>(out.size()),
                                        nullptr, nullptr);
            if (n > 0)
            {
                out.resize(n);
                return trim(out);
            }
        }
        RegCloseKey(hKey);
    }
#elif defined(__APPLE__)
    char buf[256] = {};
    size_t len    = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", buf, &len, nullptr, 0) == 0 && len > 0)
    {
        std::string s(buf, strnlen(buf, sizeof(buf)));
        return trim(s);
    }
#elif defined(__linux__)
    // /proc/cpuinfo "model name" line on x86; "Hardware"/"model name" on ARM.
    FILE* fp = fopen("/proc/cpuinfo", "r");
    if (fp)
    {
        char line[512];
        while (fgets(line, sizeof(line), fp))
        {
            const char* prefixes[] = { "model name", "Hardware", "Processor" };
            for (const char* p : prefixes)
            {
                size_t plen = strlen(p);
                if (strncmp(line, p, plen) == 0 && line[plen] == ':')
                {
                    const char* val = line + plen + 1;
                    while (*val == ' ' || *val == '\t')
                        ++val;
                    std::string s(val);
                    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                        s.pop_back();
                    fclose(fp);
                    return trim(s);
                }
            }
        }
        fclose(fp);
    }
#endif
    return "(unknown)";
}

cpu_caches get_cpu_caches()
{
    // Typical values used as a fallback for any field the OS/CPU cannot report.
    // These are reasonable for a modern x86-64 / AArch64 desktop core.
    cpu_caches caches{
        /* line_size */ 64,
        /* l1_size   */ 32 * 1024,
        /* l2_size   */ 512 * 1024,
        /* l3_size   */ 8 * 1024 * 1024,
    };

#if defined(_WIN32)
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &len);
    if (len)
    {
        std::vector<char> buf(len);
        if (GetLogicalProcessorInformationEx(
                RelationCache, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()), &len))
        {
            char* ptr = buf.data();
            char* end = buf.data() + len;
            while (ptr < end)
            {
                auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(ptr);
                if (info->Relationship == RelationCache)
                {
                    const CACHE_RELATIONSHIP& c = info->Cache;
                    // Pick the first cache reported at each level (per-core view).
                    if (c.LineSize)
                        caches.line_size = c.LineSize;
                    if (c.Level == 1 && (c.Type == CacheData || c.Type == CacheUnified))
                        caches.l1_size = c.CacheSize;
                    else if (c.Level == 2)
                        caches.l2_size = c.CacheSize;
                    else if (c.Level == 3)
                        caches.l3_size = c.CacheSize;
                }
                ptr += info->Size;
            }
        }
    }
#elif defined(__APPLE__)
    auto read_sysctl = [](const char* name, size_t& out)
    {
        uint64_t value = 0;
        size_t size    = sizeof(value);
        if (sysctlbyname(name, &value, &size, nullptr, 0) == 0 && value != 0)
            out = static_cast<size_t>(value);
    };
    read_sysctl("hw.cachelinesize", caches.line_size);
    read_sysctl("hw.l1dcachesize", caches.l1_size);
    read_sysctl("hw.l2cachesize", caches.l2_size);
    read_sysctl("hw.l3cachesize", caches.l3_size); // absent on Apple Silicon -> keeps fallback
#elif defined(__linux__)
    auto read_sysconf = [](int line_name, int size_name, size_t& line_out, size_t& size_out)
    {
        long ls = sysconf(line_name);
        if (ls > 0)
            line_out = static_cast<size_t>(ls);
        long sz = sysconf(size_name);
        if (sz > 0)
            size_out = static_cast<size_t>(sz);
    };
    size_t dummy_line = caches.line_size;
    read_sysconf(_SC_LEVEL1_DCACHE_LINESIZE, _SC_LEVEL1_DCACHE_SIZE, caches.line_size, caches.l1_size);
    read_sysconf(_SC_LEVEL2_CACHE_LINESIZE, _SC_LEVEL2_CACHE_SIZE, dummy_line, caches.l2_size);
    read_sysconf(_SC_LEVEL3_CACHE_LINESIZE, _SC_LEVEL3_CACHE_SIZE, dummy_line, caches.l3_size);
#endif

    return caches;
}

} // namespace bm