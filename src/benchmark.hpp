#pragma once
/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2023 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include <algorithm>
#include <chrono>
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
#include <memory>
#include <cstdlib>
#include <cstring>
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
extern std::chrono::nanoseconds start_time;
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
    _mm_lfence();
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
#ifdef __clang__
#ifdef __aarch64__
    uint64_t tsc;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(tsc):);
#else
    uint64_t tsc = __builtin_readcyclecounter();
#endif
#else
    uint64_t tsc = __rdtsc();
#endif
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
    return tsc;
}

extern double tsc_scale;

void calibrate_tsc();
} // namespace details

inline std::chrono::nanoseconds tsc_time()
{
    return std::chrono::nanoseconds(static_cast<uint64_t>(details::rdtsc() * details::tsc_scale));
}

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
    details::full_barrier();
    details::start_time = tsc_time();
    details::full_barrier();
}
inline std::chrono::nanoseconds bench_stop()
{
    details::full_barrier();
    std::chrono::nanoseconds stop_time = tsc_time();
    details::full_barrier();
    return stop_time - details::start_time;
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

constexpr inline const char* is_complex_str(bool is_complex)
{
    if (is_complex)
        return "complex";
    else
        return "real";
}
constexpr inline const char* inverse_str(bool inverse)
{
    if (inverse)
        return "inverse";
    else
        return "forward";
}
constexpr inline const char* inplace_str(bool inplace)
{
    if (inplace)
        return "inplace";
    else
        return "outofplace";
}

template <size_t dims>
using sizes_t = std::array<size_t, dims>;

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

template <typename real>
class fft_impl
{
public:
    virtual ~fft_impl() {}
    virtual void execute(real* out, const real* in) = 0;

    constexpr static bool valid = true;
};

class fft_impl_stub
{
public:
    constexpr static bool valid = false;
};

template <typename real>
using fft_impl_ptr = std::unique_ptr<fft_impl<real>>;

// Forward declarations
template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool inverse, bool inplace);

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims,
          bool is_complex, bool invert, bool inplace>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, std::integral_constant<bool, is_complex>,
                                  std::integral_constant<bool, invert>, std::integral_constant<bool, inplace>)
{
    if constexpr (fft_implementation<dims, real, is_complex, invert, inplace>::valid)
    {
        return fft_impl_ptr<real>(new fft_implementation<dims, real, is_complex, invert, inplace>(size));
    }
    return nullptr;
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims,
          bool is_complex, bool invert>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, std::integral_constant<bool, is_complex>,
                                  std::integral_constant<bool, invert>, bool inplace)
{
    if (inplace)
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, invert>{},
                                                        std::integral_constant<bool, true>{});
    else
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, invert>{},
                                                        std::integral_constant<bool, false>{});
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims,
          bool is_complex>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, std::integral_constant<bool, is_complex>,
                                  bool invert, bool inplace)
{
    if (invert)
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, true>{}, inplace);
    else
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, false>{}, inplace);
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, bool is_complex, bool invert, bool inplace)
{
    if (is_complex)
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, true>{}, invert,
                                                        inplace);
    else
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, false>{}, invert,
                                                        inplace);
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real>
fft_impl_ptr<real> fft_create_for(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    if (size.size() == 1)
        return fft_create_for<fft_implementation, real>(sizes_t<1>{ size[0] }, is_complex, invert, inplace);
    if (size.size() == 2)
        return fft_create_for<fft_implementation, real>(sizes_t<2>{ size[0], size[1] }, is_complex, invert,
                                                        inplace);
    if (size.size() == 3)
        return fft_create_for<fft_implementation, real>(sizes_t<3>{ size[0], size[1], size[2] }, is_complex,
                                                        invert, inplace);
    return nullptr;
}

extern template fft_impl_ptr<float> fft_create<float>(const std::vector<size_t>& size, bool is_complex,
                                                      bool inverse, bool inplace);
extern template fft_impl_ptr<double> fft_create<double>(const std::vector<size_t>& size, bool is_complex,
                                                        bool inverse, bool inplace);

std::string fft_name();

extern bool avx2only;