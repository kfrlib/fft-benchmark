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

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define PRNG_SSE2
#include <emmintrin.h>
#elif defined(__ARM_NEON) || defined(__aarch64__)
#define PRNG_NEON
#include <arm_neon.h>
#else
#define PRNG_SCALAR
#endif

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
#include <numeric>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#if __has_include(<cpuid.h>)
#include <cpuid.h>
#endif
#endif

#if defined __clang__ || defined __GNUC__
#define BENCH_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define BENCH_INLINE __forceinline
#else
#define BENCH_INLINE inline
#endif

#if defined __clang__ || defined __GNUC__
#define BENCH_LAMBDA_INLINE __attribute__((__always_inline__))
#define BENCH_NOINLINE __attribute__((__noinline__))
#else
#define BENCH_LAMBDA_INLINE [[msvc::forceinline]]
#define BENCH_NOINLINE __declspec(noinline)
#endif

namespace bm
{

void* page_aligned_alloc(size_t size);
void page_aligned_free(void* ptr, size_t size);

template <typename T>
BENCH_INLINE T* aligned_malloc(size_t size)
{
    return reinterpret_cast<T*>(page_aligned_alloc(size * sizeof(T)));
}
template <typename T>
BENCH_INLINE void aligned_free(T* aligned_ptr, size_t size)
{
    page_aligned_free(reinterpret_cast<void*>(aligned_ptr), size * sizeof(T));
}

template <typename T>
struct aligned_deleter
{
    size_t size;
    void operator()(T* ptr) const { aligned_free(ptr, size); }
};

template <typename T>
using aligned_ptr = std::unique_ptr<T[], aligned_deleter<T>>;

template <typename T>
BENCH_INLINE aligned_ptr<T> aligned_malloc_raii(size_t size)
{
    return aligned_ptr<T>(aligned_malloc<T>(size), aligned_deleter<T>{ size });
}

template <typename T>
struct aligned_allocator
{
    using value_type = T;
    template <typename U>
    struct rebind
    {
        using other = aligned_allocator<U>;
    };
    aligned_allocator() noexcept {}
    template <typename U>
    aligned_allocator(const aligned_allocator<U>&) noexcept
    {
    }
    T* allocate(size_t n) { return aligned_malloc<T>(n); }
    void deallocate(T* p, size_t n) { aligned_free<T>(p, n); }
    bool operator==(const aligned_allocator&) const { return true; }
    bool operator!=(const aligned_allocator&) const { return false; }
};

void use_from_outside(const char volatile*);

BENCH_INLINE void dont_optimize(const void* in)
{
#if defined __clang__ || __GNUC__
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

template <typename T>
constexpr inline const char* type_name = "float";

template <>
constexpr inline const char* type_name<double> = "double";

// OS-specific fallback used when CPUID (or its brand-string leaf) is not
// available. Implemented in benchmark.cpp.
std::string cpu_name_from_os();

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
        return cpu_name_from_os();

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
    std::string name              = trim(std::string(std::data(result), std::size(result)));
    // Some virtualized environments return an empty/whitespace brand string
    // even though the leaf is present; fall back to the OS in that case.
    return name.empty() ? cpu_name_from_os() : name;
#else
    return cpu_name_from_os();
#endif
}

inline std::string execfile(std::string command)
{
    size_t pos = command.find_last_of("/\\");
    command    = command.substr(pos == std::string::npos ? 0 : pos + 1);
    if (command.substr(command.size() - 4) == ".exe")
        command = command.substr(0, command.size() - 4);
    return command;
}

// defaults to 2 (2nd core if hyperthreading, 3rd core otherwise)
void run_on_core(int core);

struct benchmark_scope
{
    benchmark_scope();
    ~benchmark_scope();
};

static void bench_start();
static std::chrono::nanoseconds bench_stop();
int get_ideal_core();

namespace details
{
BENCH_INLINE void full_barrier()
{
#if defined(__x86_64__) || defined(_M_X64)
    _mm_lfence();
#else
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}
#if defined(USE_OS_TIME)
extern std::chrono::nanoseconds start_time;
#else
extern uint64_t start_time;
#endif
} // namespace details

#if defined(_WIN32)

namespace details
{
double get_qpc_scale();
uint64_t get_qpc_current();
extern double qpc_scale;
} // namespace details
BENCH_INLINE std::chrono::nanoseconds os_time()
{
    return std::chrono::nanoseconds(static_cast<uint64_t>(details::get_qpc_current() * details::qpc_scale));
}
// in nanoseconds
BENCH_INLINE double os_time_resolution() { return details::get_qpc_scale(); }

#else

#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

BENCH_INLINE std::chrono::nanoseconds os_time()
{
    timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return std::chrono::nanoseconds(time.tv_sec * 1000'000'000ull + time.tv_nsec);
}
BENCH_INLINE double os_time_resolution()
{
    timespec time;
    clock_getres(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000'000'000.0 + time.tv_nsec;
}

#endif

namespace details
{

BENCH_INLINE uint64_t rdtsc()
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

BENCH_INLINE static uint64_t tsc_time() { return details::rdtsc(); }

BENCH_INLINE static double tsc_resolution()
{
    details::calibrate_tsc();
    return details::tsc_scale;
}

#if defined(USE_OS_TIME)
BENCH_INLINE static void bench_start() { details::start_time = os_time(); }
BENCH_INLINE static std::chrono::nanoseconds bench_stop()
{
    std::chrono::nanoseconds stop_time = os_time();
    return stop_time - details::start_time;
}
#else

BENCH_INLINE static void bench_start() { details::start_time = tsc_time(); }
BENCH_INLINE static std::chrono::nanoseconds bench_stop()
{
    uint64_t stop_time = tsc_time();
    return std::chrono::nanoseconds(uint64_t((stop_time - details::start_time) * tsc_resolution()));
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

struct Percentiles
{
    double p1;
    double p25;
    double p50_median;
    double p75;
};

inline double sample_at(double pos, const std::vector<double>& measures)
{
    if (measures.empty())
        return -1.0;
    double idx       = pos * (measures.size() - 1);
    size_t idx_below = static_cast<size_t>(std::floor(idx));
    size_t idx_above = static_cast<size_t>(std::ceil(idx));
    if (idx_below == idx_above)
        return measures[idx_below];
    double weight_above = idx - idx_below;
    double weight_below = 1.0 - weight_above;
    return measures[idx_below] * weight_below + measures[idx_above] * weight_above;
}

template <bool sorted = false, typename Alloc>
static Percentiles get_percentiles(std::vector<double, Alloc>& measures)
{
    if constexpr (!sorted)
        std::sort(measures.begin(), measures.end());
    Percentiles result;

    result.p50_median = sample_at(0.5, measures);
    result.p1         = sample_at(0.01, measures);
    result.p25        = sample_at(0.25, measures);
    result.p75        = sample_at(0.75, measures);
    return result;
}

struct batch_result
{
    struct func_result
    {
        double mean_p1         = 0.0;
        double mean_median     = 0.0;
        uint64_t fastest_count = 0;
    };
    size_t n_funcs  = 0;
    uint64_t epochs = 0, iterations = 0;

    std::vector<func_result> funcs; // [n_funcs]
    uint64_t tie = 0;

    size_t winner = 0; // index of function with best mean P1

    uint64_t flops = 0; // flops per call for the reference algorithm (0 = not provided)
};

inline void batch_print(const batch_result& result, const char* label, const std::vector<const char*>& names)
{
    printf("\033[36m");
    printf("Label: %s\n", label);

    const double n = static_cast<double>(result.epochs * result.iterations);

    // Precompute sum of all P1 values to derive per-function relative speed
    double total_p1 = 0.0;
    for (size_t i = 0; i < result.n_funcs; ++i)
        total_p1 += result.funcs[i].mean_p1;

    // Compute flops/s per function when a reference flop count is provided
    const bool show_flops = (result.flops != 0);
    double fps_scale      = 1.0;
    const char* fps_unit  = "flops/s";
    std::vector<double> fps_values;
    if (show_flops)
    {
        fps_values.resize(result.n_funcs);
        double max_fps = 0.0;
        for (size_t i = 0; i < result.n_funcs; ++i)
        {
            // mean_p1 is in microseconds; flops/s = flops / (us * 1e-6)
            fps_values[i] = static_cast<double>(result.flops) * 1e6 / result.funcs[i].mean_p1;
            if (fps_values[i] > max_fps)
                max_fps = fps_values[i];
        }
        // Pick the largest SI prefix that keeps the maximum value >= 10
        // (guarantees the displayed range is roughly [10, 9999])
        if (max_fps >= 1e13)
        {
            fps_scale = 1e12;
            fps_unit  = "Tflops/s";
        }
        else if (max_fps >= 1e10)
        {
            fps_scale = 1e9;
            fps_unit  = "Gflops/s";
        }
        else if (max_fps >= 1e7)
        {
            fps_scale = 1e6;
            fps_unit  = "Mflops/s";
        }
        else if (max_fps >= 1e4)
        {
            fps_scale = 1e3;
            fps_unit  = "kflops/s";
        }
    }

    if (show_flops)
    {
        printf("%-10s %12s %12s %12s %12s %12s %12s %12s\n", "Func", "Best(P1)", "Median(P50)", "P50/P1",
               "Vs.Winner", "Vs.Others", "Win%", fps_unit);
        printf("%-10s %12s %12s %12s %12s %12s %12s %12s\n", "----", "--------", "-----------", "------",
               "---------", "---------", "----", "--------");
    }
    else
    {
        printf("%-10s %12s %12s %12s %12s %12s %12s\n", "Func", "Best(P1)", "Median(P50)", "P50/P1",
               "Vs.Winner", "Vs.Others", "Win%");
        printf("%-10s %12s %12s %12s %12s %12s %12s\n", "----", "--------", "-----------", "------",
               "---------", "---------", "----");
    }

    // sort indices by mean_p1 ascending so the winner is always first
    std::vector<size_t> order(result.n_funcs);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return result.funcs[a].mean_p1 < result.funcs[b].mean_p1; });

    const char* const* name_data = std::data(names);
    for (size_t rank = 0; rank < result.n_funcs; ++rank)
    {
        const size_t idx = order[rank];
        const char* name = name_data[idx];
        // avg P1 of all functions except this one
        double avg_others  = (result.n_funcs > 1)
                                 ? (total_p1 - result.funcs[idx].mean_p1) / (result.n_funcs - 1)
                                 : result.funcs[idx].mean_p1;
        double rel_speed   = avg_others / result.funcs[idx].mean_p1 * 100.0;
        double vs_winner   = result.funcs[result.winner].mean_p1 / result.funcs[idx].mean_p1 * 100.0;
        double pct_fastest = 100.0 * result.funcs[idx].fastest_count / n;
        if (rank == 0)
            printf("\033[1;32m");
        if (show_flops)
            printf("%-10s %10.3fus %10.3fus %11.1f%% %11.1f%% %11.1f%% %11.1f%% %11.1f\n", name,
                   result.funcs[idx].mean_p1, result.funcs[idx].mean_median,
                   (result.funcs[idx].mean_median / result.funcs[idx].mean_p1 - 1) * 100, vs_winner,
                   rel_speed, pct_fastest, fps_values[idx] / fps_scale);
        else
            printf("%-10s %10.3fus %10.3fus %11.1f%% %11.1f%% %11.1f%% %11.1f%%\n", name,
                   result.funcs[idx].mean_p1, result.funcs[idx].mean_median,
                   (result.funcs[idx].mean_median / result.funcs[idx].mean_p1 - 1) * 100, vs_winner,
                   rel_speed, pct_fastest);
        if (rank == 0)
            printf("\033[0m\033[36m");
    }
    printf("--------------------------------------------------------------------------\n");
    printf("\033[0m\n");
}

namespace details
{
template <size_t N, size_t... Is, typename Tuple, typename Fn>
BENCH_INLINE void apply_fwd(std::index_sequence<Is...>, Tuple&& t, Fn&& fn)
{
    (fn(std::get<Is>(std::forward<Tuple>(t)), Is), ...);
}

template <size_t N, size_t... Is, typename Tuple, typename Fn>
BENCH_INLINE void apply_rev(std::index_sequence<Is...>, Tuple&& t, Fn&& fn)
{
    (fn(std::get<N - 1 - Is>(std::forward<Tuple>(t)), N - 1 - Is), ...);
}
} // namespace details

template <typename... Funcs, typename OnEpoch, typename PreMeasure, typename PostMeasure>
batch_result batch_benchmark(uint64_t epochs, uint64_t iterations, std::tuple<Funcs...> funcs,
                             OnEpoch&& on_epoch, PreMeasure&& pre_measure, PostMeasure&& post_measure,
                             uint64_t flops = 0)
{
    constexpr size_t N = sizeof...(Funcs);
    constexpr auto seq = std::make_index_sequence<N>{};

    auto measure_one = [&](auto&& func, double& out) BENCH_LAMBDA_INLINE
    {
        pre_measure();
        bench_start();
        func();
        out = bench_stop().count() * 1e-3; // convert to microseconds
        post_measure();
    };

    std::vector<std::vector<double>> durations(N, std::vector<double>(static_cast<size_t>(iterations)));
    double dummy;

    // warm up
    for (uint64_t iter = 0; iter < iterations; ++iter)
        details::apply_fwd<N>(seq, funcs,
                              [&](auto&& fn, size_t) BENCH_LAMBDA_INLINE { measure_one(fn, dummy); });
    for (uint64_t iter = 0; iter < iterations; ++iter)
        details::apply_rev<N>(seq, funcs,
                              [&](auto&& fn, size_t) BENCH_LAMBDA_INLINE { measure_one(fn, dummy); });

    batch_result result;
    result.n_funcs    = N;
    result.epochs     = epochs;
    result.iterations = iterations;
    result.flops      = flops;
    result.funcs.assign(N, {});

    // measure
    for (uint64_t epoch = 0; epoch < epochs; ++epoch)
    {
        on_epoch();
        if (epoch % 2 == 0)
        {
            for (uint64_t iter = 0; iter < iterations; ++iter)
                details::apply_fwd<N>(seq, funcs, [&](auto&& fn, size_t i) BENCH_LAMBDA_INLINE
                                      { measure_one(fn, durations[i][iter]); });
        }
        else
        {
            for (uint64_t iter = 0; iter < iterations; ++iter)
                details::apply_rev<N>(seq, funcs, [&](auto&& fn, size_t i) BENCH_LAMBDA_INLINE
                                      { measure_one(fn, durations[i][iter]); });
        }

        // tally per-iteration winner
        for (uint64_t iter = 0; iter < iterations; ++iter)
        {
            size_t best_idx = 0;
            double best_val = durations[0][iter];
            for (size_t i = 1; i < N; ++i)
                if (durations[i][iter] < best_val)
                {
                    best_val = durations[i][iter];
                    best_idx = i;
                }
            bool all_tied = true;
            for (size_t i = 0; i < N; ++i)
                if (durations[i][iter] > best_val * 1.01)
                {
                    all_tied = false;
                    break;
                }
            if (all_tied)
                ++result.tie;
            else
                ++result.funcs[best_idx].fastest_count;
        }

        for (size_t i = 0; i < N; ++i)
        {
            Percentiles p = get_percentiles(durations[i]);
            result.funcs[i].mean_p1 += p.p1 / epochs;
            result.funcs[i].mean_median += p.p50_median / epochs;
        }
    }

    result.winner = 0;
    for (size_t i = 1; i < N; ++i)
        if (result.funcs[i].mean_p1 < result.funcs[result.winner].mean_p1)
            result.winner = i;

    return result;
}

namespace prng
{

#if defined(__AVX2__)
// ==========================================
// AVX2 Native Path
// ==========================================
static __m256i lcg_state =
    _mm256_setr_epi32(123456789, 234567891, 345678912, 456789123, 567891234, 678912345, 789123456, 891234567);
static const __m256i LCG_A  = _mm256_set1_epi32(1103515245);
static const __m256i LCG_C  = _mm256_set1_epi32(12345);
static const __m256i MASK31 = _mm256_set1_epi32(0x7fffffff);
static const __m256 SCALE   = _mm256_set1_ps(1.0f / 1073741824.0f);
static const __m256 OFFSET  = _mm256_set1_ps(-1.0f);

struct float8_vec
{
    __m256 v;
};

inline float8_vec lcg_next_8()
{
    lcg_state = _mm256_add_epi32(_mm256_mullo_epi32(lcg_state, LCG_A), LCG_C);
    lcg_state = _mm256_and_si256(lcg_state, MASK31);
    __m256 f  = _mm256_cvtepi32_ps(lcg_state);
    return float8_vec{ _mm256_fmadd_ps(f, SCALE, OFFSET) };
}

inline void storeu_float8(float* dest, const float8_vec& vec) { _mm256_storeu_ps(dest, vec.v); }
inline void store_float8(float* dest, const float8_vec& vec) { _mm256_store_ps(dest, vec.v); }

#elif defined(PRNG_SSE2)
// ==========================================
// SSE2 Path (Emulated 32-bit Integer Multiply)
// ==========================================
static __m128i lcg_state_0      = _mm_setr_epi32(123456789, 234567891, 345678912, 456789123);
static __m128i lcg_state_1      = _mm_setr_epi32(567891234, 678912345, 789123456, 891234567);
static const __m128i LCG_A_128  = _mm_set1_epi32(1103515245);
static const __m128i LCG_C_128  = _mm_set1_epi32(12345);
static const __m128i MASK31_128 = _mm_set1_epi32(0x7fffffff);
static const __m128 SCALE_128   = _mm_set1_ps(1.0f / 1073741824.0f);
static const __m128 OFFSET_128  = _mm_set1_ps(-1.0f);

struct float8_vec
{
    __m128 v0, v1;
};

// SSE2 lacks _mm_mullo_epi32 (introduced in SSE4.1). Emulating it efficiently here:
inline __m128i mm_mullo_epi32_sse2(__m128i a, __m128i b)
{
    __m128i p02 = _mm_mul_epu32(a, b);
    __m128i p13 = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4));
    return _mm_unpacklo_epi64(_mm_unpacklo_epi32(p02, p13), _mm_unpackhi_epi32(p02, p13));
}

inline __m128 lcg_next_4_sse2(__m128i& state)
{
    state    = _mm_add_epi32(mm_mullo_epi32_sse2(state, LCG_A_128), LCG_C_128);
    state    = _mm_and_si128(state, MASK31_128);
    __m128 f = _mm_cvtepi32_ps(state);
    return _mm_add_ps(_mm_mul_ps(f, SCALE_128), OFFSET_128); // FMA Emulation
}

inline float8_vec lcg_next_8()
{
    return float8_vec{ lcg_next_4_sse2(lcg_state_0), lcg_next_4_sse2(lcg_state_1) };
}

inline void storeu_float8(float* dest, const float8_vec& vec)
{
    _mm_storeu_ps(dest, vec.v0);
    _mm_storeu_ps(dest + 4, vec.v1);
}
inline void store_float8(float* dest, const float8_vec& vec)
{
    _mm_store_ps(dest, vec.v0);
    _mm_store_ps(dest + 4, vec.v1);
}

#elif defined(PRNG_NEON)
// ==========================================
// ARM64 NEON Path
// ==========================================
static int32x4_t lcg_state_0         = { 123456789, 234567891, 345678912, 456789123 };
static int32x4_t lcg_state_1         = { 567891234, 678912345, 789123456, 891234567 };
static const int32x4_t LCG_A_NEON    = vdupq_n_s32(1103515245);
static const int32x4_t LCG_C_NEON    = vdupq_n_s32(12345);
static const int32x4_t MASK31_NEON   = vdupq_n_s32(0x7fffffff);
static const float32x4_t SCALE_NEON  = vdupq_n_f32(1.0f / 1073741824.0f);
static const float32x4_t OFFSET_NEON = vdupq_n_f32(-1.0f);

struct float8_vec
{
    float32x4_t v0, v1;
};

inline float32x4_t lcg_next_4_neon(int32x4_t& state)
{
    state         = vmlaq_s32(LCG_C_NEON, state, LCG_A_NEON); // state = state * A + C
    state         = vandq_s32(state, MASK31_NEON);
    float32x4_t f = vcvtq_f32_s32(state);
    return vfmaq_f32(OFFSET_NEON, f, SCALE_NEON); // f * SCALE + OFFSET
}

inline float8_vec lcg_next_8()
{
    return float8_vec{ lcg_next_4_neon(lcg_state_0), lcg_next_4_neon(lcg_state_1) };
}

inline void storeu_float8(float* dest, const float8_vec& vec)
{
    vst1q_f32(dest, vec.v0);
    vst1q_f32(dest + 4, vec.v1);
}
inline void store_float8(float* dest, const float8_vec& vec)
{
    vst1q_f32(dest, vec.v0);
    vst1q_f32(dest + 4, vec.v1);
}

#else
// ==========================================
// Fast Scalar Fallback Path
// ==========================================
static int32_t lcg_state[8] = { 123456789, 234567891, 345678912, 456789123,
                                567891234, 678912345, 789123456, 891234567 };

struct float8_vec
{
    float v[8];
};

inline float8_vec lcg_next_8()
{
    float8_vec res;
    for (int i = 0; i < 8; ++i)
    {
        lcg_state[i] = (lcg_state[i] * 1103515245 + 12345) & 0x7fffffff;
        res.v[i]     = static_cast<float>(lcg_state[i]) * (1.0f / 1073741824.0f) - 1.0f;
    }
    return res;
}

inline void storeu_float8(float* dest, const float8_vec& vec)
{
    for (int i = 0; i < 8; ++i)
        dest[i] = vec.v[i];
}
inline void store_float8(float* dest, const float8_vec& vec)
{
    for (int i = 0; i < 8; ++i)
        dest[i] = vec.v[i];
}
#endif

// ==========================================
// Unified Execution Logic
// ==========================================
template <bool tail, typename T>
void generate(T* data, size_t size)
{
    size_t i = 0;

    if constexpr (std::is_same_v<T, float>)
    {
        // Fast path: store 8 floats at a time directly
        const size_t vec_count = size / 8;
        float* fdata           = reinterpret_cast<float*>(data);

        for (size_t v = 0; v < vec_count; ++v)
        {
            float8_vec vals = lcg_next_8();
            storeu_float8(fdata + v * 8, vals);
        }
        i = vec_count * 8;
    }
    else
    {
        // General path: generate 8 floats, cast to T
        alignas(32) float tmp[8];
        const size_t vec_count = size / 8;

        for (size_t v = 0; v < vec_count; ++v)
        {
            float8_vec vals = lcg_next_8();
            store_float8(tmp, vals);
            for (int j = 0; j < 8; ++j)
                data[v * 8 + j] = static_cast<T>(tmp[j]);
        }
        i = vec_count * 8;
    }

    if constexpr (tail)
    {
        // Scalar tail: handle remaining elements (< 8)
        if (i < size)
        {
            alignas(32) float tmp[8];
            float8_vec vals = lcg_next_8();
            store_float8(tmp, vals);
            for (size_t j = 0; i < size; ++i, ++j)
                data[i] = static_cast<T>(tmp[j]);
        }
    }
}

} // namespace prng

struct cpu_caches
{
    size_t line_size;
    size_t l1_size;
    size_t l2_size;
    size_t l3_size;
};

// Queries the CPU/OS for cache parameters. Uses platform APIs where available
// and falls back to typical values for any field that cannot be determined.
cpu_caches get_cpu_caches();

inline std::string cpu_caches_string(const cpu_caches& caches)
{
    return "L1: " + std::to_string(caches.l1_size / 1024) +
           "KB, L2: " + std::to_string(caches.l2_size / 1024) +
           "KB, L3: " + std::to_string(caches.l3_size / 1024) +
           "KB, Line: " + std::to_string(caches.line_size) + "B";
}

} // namespace bm