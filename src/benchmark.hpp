#pragma once
/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

#ifdef _WIN32
#include <intrin.h>
#else
#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#else
#include <arm_neon.h>
#endif
#endif
#include <array>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#if __has_include(<cpuid.h>)
#include <cpuid.h>
#endif
#endif

template <typename T>
inline T* aligned_malloc(size_t size, size_t alignment = 64)
{
    void* ptr = malloc(size * sizeof(T) + (alignment - 1) + sizeof(void*));
    if (ptr == NULL)
        return NULL;
    void* aligned_ptr         = (void*)(((uintptr_t)ptr + sizeof(void*) + alignment - 1) & ~(alignment - 1));
    ((void**)aligned_ptr)[-1] = ptr;
    return static_cast<T*>(aligned_ptr);
}
inline void aligned_free(void* aligned_ptr) { free(((void**)aligned_ptr)[-1]); }

template <typename T>
struct aligned_allocator
{
    using value_type = T;
    aligned_allocator() noexcept {}
    template <typename U>
    aligned_allocator(const aligned_allocator<U>&) noexcept
    {
    }
    T* allocate(size_t n) { return aligned_malloc<T>(n); }
    void deallocate(T* p, size_t) { aligned_free(p); }
    bool operator==(const aligned_allocator&) const { return true; }
    bool operator!=(const aligned_allocator&) const { return false; }
};

void use_from_outside(const char volatile*);

inline void dont_optimize(const void* in)
{
#ifdef __GNUC__
    asm volatile("" : : "g"(in) : "memory");
#else
    use_from_outside(reinterpret_cast<const char volatile*>(in));
    _ReadWriteBarrier();
#endif
}

inline std::string trim(std::string s)
{
    while (!s.empty() && s.front() <= ' ')
        s.erase(0, 1);
    while (!s.empty() && s.back() <= ' ')
        s.erase(s.size() - 1, 1);
    return s;
}

inline std::string cpu_name()
{
#if defined(__x86_64__) || defined(_M_X64)
#if defined __GNUC__ || defined __clang__
    uint32_t data[12];
#else
    int data[12];
#endif
    char result[sizeof(data) + 1];

#if defined __GNUC__ || defined __clang__
    __cpuid(0x80000000, data[0], data[1], data[2], data[3]);
#else
    __cpuid(data, 0x80000000);
#endif

    if (data[0] < 0x80000004)
        return "(unknown)";

#if defined __GNUC__ || defined __clang__
    __cpuid(0x80000002, data[0], data[1], data[2], data[3]);
    __cpuid(0x80000003, data[4], data[5], data[6], data[7]);
    __cpuid(0x80000004, data[8], data[9], data[10], data[11]);
#else
    __cpuid(data, 0x80000002);
    __cpuid(data + 4, 0x80000003);
    __cpuid(data + 8, 0x80000004);
#endif

    std::memcpy(result, data, sizeof(data));
    result[std::size(result) - 1] = 0;
    return trim(std::string(std::data(result), std::size(result)));
#else
    return "(unknown)";
#endif
}

// defaults to 0 (1st core)
void run_on_core(int core);

struct benchmark_scope
{
    benchmark_scope();
    ~benchmark_scope();
};

void bench_start();
std::chrono::nanoseconds bench_stop();

namespace details
{
inline void full_barrier()
{
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}
extern uint64_t start_time;
} // namespace details

#if defined(_WIN32)

namespace details
{
double get_qpc_scale();
uint64_t get_qpc_current();
extern double qpc_scale;
} // namespace details
inline std::chrono::nanoseconds os_time()
{
    return std::chrono::nanoseconds(static_cast<uint64_t>(details::get_qpc_current() * details::qpc_scale));
}
// in nanoseconds
inline double os_time_resolution() { return details::get_qpc_scale(); }

#else

#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

inline std::chrono::nanoseconds os_time()
{
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return std::chrono::nanoseconds(time.tv_sec * 1000'000'000ull + time.tv_nsec);
}
inline double os_time_resolution()
{
    timespec time;
    clock_getres(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000'000'000.0 + time.tv_nsec;
}

#endif

namespace details
{

inline uint64_t rdtsc()
{
#if defined(__x86_64__) || defined(_M_X64)
    // lfence: execution serialization — drains the out-of-order engine so that
    // all prior instructions retire before RDTSC, and RDTSC completes before
    // any subsequent instruction starts.
    _mm_lfence();
#elif defined(__aarch64__)
    // isb: instruction synchronization barrier — flushes the pipeline so that
    // all prior instructions are complete before the counter is read.
    // dmb (what atomic_thread_fence emits) only orders *memory* accesses and
    // does not prevent the CPU from speculating across it.
    asm volatile("isb" ::: "memory");
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif

#if defined(__aarch64__)
    uint64_t tsc;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(tsc));
#elif defined(__clang__)
    uint64_t tsc = __builtin_readcyclecounter();
#else
    uint64_t tsc = __rdtsc();
#endif

#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
#elif defined(__aarch64__)
    asm volatile("isb" ::: "memory");
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
    return tsc;
}

extern double tsc_scale;

void calibrate_tsc();
} // namespace details

inline uint64_t tsc_time() { return details::rdtsc(); }

inline double tsc_resolution() { return details::tsc_scale; }

#if defined(USE_OS_TIME)
inline void bench_start()
{
    details::full_barrier();
    details::start_time = os_time();
    details::full_barrier();
}
inline std::chrono::nanoseconds bench_stop()
{
    details::full_barrier();
    std::chrono::nanoseconds stop_time = os_time();
    details::full_barrier();
    return stop_time - details::start_time;
}
#else

inline void bench_start()
{
    // rdtsc() already contains lfence;read;lfence — no extra barriers needed.
    details::start_time = tsc_time();
}
inline std::chrono::nanoseconds bench_stop()
{
    // rdtsc() already contains lfence;read;lfence — no extra barriers needed.
    uint64_t stop_time = tsc_time();
    return std::chrono::nanoseconds(uint64_t((stop_time - details::start_time) * details::tsc_scale));
}
#endif

template <typename T>
static T get_average(const std::vector<T>& measures)
{
    T sum = T();
    for (size_t i = 0; i < measures.size(); i++)
        sum += measures[i];
    return sum / measures.size();
}

template <typename T>
static T get_minimum(const std::vector<T>& measures)
{
    return *std::min_element(measures.begin(), measures.end());
}

template <typename T>
static T get_median(std::vector<T>& measures)
{
    std::sort(measures.begin(), measures.end());
    const size_t middle = measures.size() / 2;

    if (measures.size() % 2 == 1)
        return measures[middle];
    else
        return static_cast<T>((measures[middle - 1] + measures[middle]) / 2);
}

#define PICK                                                                                                 \
    template <typename T1, typename T2>                                                                      \
    using pick_t = std::conditional_t<sizeof(real) == 4, T1, T2>;                                            \
    template <typename T1, typename T2>                                                                      \
    static constexpr auto pick(T1 v1, T2 v2)                                                                 \
    {                                                                                                        \
        if constexpr (sizeof(real) == 4)                                                                     \
            return v1;                                                                                       \
        else                                                                                                 \
            return v2;                                                                                       \
    }

template <typename T>
constexpr inline const char* type_name = "float";

template <>
constexpr inline const char* type_name<double> = "double";

template <size_t dims>
using sizes_t = std::array<size_t, dims>;

template <typename real>
void fill_random(real* in, size_t size)
{
    for (size_t i = 0; i < size; i++)
        in[i] = static_cast<real>(((double)rand() / RAND_MAX) * 2.0 - 1.0);
}

template <typename real>
static double rms(const real* a, const double* ref, size_t size)
{
    double sum = 0;
    for (size_t i = 0; i < size; i++)
    {
        double diff = a[i] - ref[i];
        sum += diff * diff;
    }
    return std::sqrt(sum / size);
}

inline size_t parse_number(std::string_view& s)
{
    size_t n = s.find_first_not_of("0123456789");
    if (n == 0)
        return 0;
    if (n == std::string_view::npos)
        n = s.size();
    size_t result;
    std::from_chars(s.data(), s.data() + n, result);
    s = s.substr(n);
    return result;
}

inline size_t product(std::vector<size_t> sizes)
{
    size_t result = sizes[0];
    for (size_t i = 1; i < sizes.size(); ++i)
        result *= sizes[i];
    return result;
}

inline std::string sizes_to_string(std::vector<size_t> sizes)
{
    std::string result;
    for (size_t n : sizes)
    {
        if (!result.empty())
            result += "x";

        char buf[32];
        size_t wr = std::snprintf(buf, sizeof(buf), "%zu", n);
        result += std::string_view(std::begin(buf), std::min(wr, sizeof(buf)));
    }
    return result;
}

inline std::vector<size_t> parse_size(std::string_view s)
{
    std::vector<size_t> result;
    if (s.empty())
        return result;
    while (size_t n = parse_number(s))
    {
        result.push_back(n);
        if (s.empty())
            return result;
        if (s[0] == 'x')
            s = s.substr(1);
        else
            return {};
    }
    return result;
}

inline std::vector<bool> to_vector_bool(std::string_view s)
{
    using namespace std::string_view_literals;

    std::vector<bool> result;
    for (char c : s)
    {
        if ("yY1"sv.find_first_of(c) != std::string_view::npos)
        {
            result.push_back(true);
        }
        else if ("nN0"sv.find_first_of(c) != std::string_view::npos)
        {
            result.push_back(false);
        }
    }
    return result;
}

inline std::string execfile(std::string command)
{
    size_t pos = command.find_last_of("/\\");
    command    = command.substr(pos == std::string::npos ? 0 : pos + 1);
    if (command.substr(command.size() - 4) == ".exe")
        command = command.substr(0, command.size() - 4);
    return command;
}

extern bool avx2only;