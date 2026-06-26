/**
 * FFT bencmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#define NOMINMAX 1

#include <cstring>
#include <inttypes.h>

#include "fft_benchmark.hpp"
#include <cmath>
#include <numeric>
#include <string>
#include <utility>

#include "json.hpp"
#include <fstream>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

extern "C" const double dft_testvector_complex_input60[];
extern "C" const double dft_testvector_complex_output60[];
extern "C" const double dft_testvector_complex_input61[];
extern "C" const double dft_testvector_complex_output61[];
extern "C" const double dft_testvector_complex_input62[];
extern "C" const double dft_testvector_complex_output62[];
extern "C" const double dft_testvector_complex_input64[];
extern "C" const double dft_testvector_complex_output64[];
extern "C" const double dft_testvector_real_input60[];
extern "C" const double dft_testvector_real_output60[];
extern "C" const double dft_testvector_real_input61[];
extern "C" const double dft_testvector_real_output61[];
extern "C" const double dft_testvector_real_input62[];
extern "C" const double dft_testvector_real_output62[];
extern "C" const double dft_testvector_real_input64[];
extern "C" const double dft_testvector_real_output64[];

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

[[clang::noinline]] void synthetic_workload(long long iterations)
{
#ifdef __AVX512F__
    __m512 a = _mm512_set1_ps(1.00001f);
    __m512 b = _mm512_set1_ps(0.99999f);
    __m512 c = _mm512_set1_ps(0.50000f);
    __m512 d = _mm512_set1_ps(1.41421f);

    for (long long i = 0; i < iterations; i++)
    {
        a = _mm512_fmadd_ps(a, b, c); // port 0
        b = _mm512_fmadd_ps(b, c, d); // port 1
        c = _mm512_fmadd_ps(c, d, a); // back-dep on a
        d = _mm512_fmadd_ps(d, a, b); // back-dep on a,b
    }

    volatile float sink =
        _mm512_cvtss_f32(a) + _mm512_cvtss_f32(b) + _mm512_cvtss_f32(c) + _mm512_cvtss_f32(d);
    (void)sink;
#elif defined __AVX2__
    __m256 a = _mm256_set1_ps(1.00001f);
    __m256 b = _mm256_set1_ps(0.99999f);
    __m256 c = _mm256_set1_ps(0.50000f);
    __m256 d = _mm256_set1_ps(1.41421f);

    for (long long i = 0; i < iterations; i++)
    {
        a = _mm256_fmadd_ps(a, b, c); // port 0
        b = _mm256_fmadd_ps(b, c, d); // port 1
        c = _mm256_fmadd_ps(c, d, a); // back-dep on a
        d = _mm256_fmadd_ps(d, a, b); // back-dep on a,b
    }

    volatile float sink =
        _mm256_cvtss_f32(a) + _mm256_cvtss_f32(b) + _mm256_cvtss_f32(c) + _mm256_cvtss_f32(d);
    (void)sink;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t a = vdupq_n_f32(1.00001f);
    float32x4_t b = vdupq_n_f32(0.99999f);
    float32x4_t c = vdupq_n_f32(0.50000f);
    float32x4_t d = vdupq_n_f32(1.41421f);

    for (long long i = 0; i < iterations; i++)
    {
        a = vfmaq_f32(a, b, c); // FMA0
        b = vfmaq_f32(b, c, d); // FMA1
        c = vfmaq_f32(c, d, a); // back-dep on a
        d = vfmaq_f32(d, a, b); // back-dep on a,b
    }

    volatile float sink =
        vgetq_lane_f32(a, 0) + vgetq_lane_f32(b, 0) + vgetq_lane_f32(c, 0) + vgetq_lane_f32(d, 0);
    (void)sink;
#else
#error "No synthetic workload implementation for this architecture"
#endif
}

template <typename real>
BENCH_INLINE std::chrono::nanoseconds batch(fft_impl<real>* fft, real* out, const real* in,
                                            unsigned batch_size, bool inplace, size_t real_size)
{
    std::chrono::nanoseconds result;
    if (inplace)
    {
        if (batch_size != 1)
        {
            return std::chrono::nanoseconds(0); // In-place batch > 1 not supported
        }
        memcpy(out, in, sizeof(real) * real_size);
        bench_start();
        fft->execute(out, out);
        dont_optimize(out);
        result = bench_stop();
    }
    else
    {
        bench_start();
        for (unsigned i = 0; i < batch_size; ++i)
        {
            fft->execute(out, in);
            dont_optimize(out);
        }
        result = bench_stop();
    }
    return result;
}

template <typename real>
struct fft_benchmark_runner
{

    // The reference test vectors use the unnormalized convention:
    //   forward reference  = DFT sum (no 1/N)
    //   inverse reference  = original time-domain signal (the forward reference
    //                        is the spectrum of this signal)
    // Each library reports its normalization convention via fft->scaling().
    // To compare without ever rescaling the library output, we scale the
    // reference to match what the library produces. Returns the factor by
    // which the reference must be multiplied.
    static double reference_scale(fft_scaling scaling, bool inverse, size_t size)
    {
        switch (scaling)
        {
        case fft_scaling::none:
            // forward: unnormalized  -> matches reference (×1)
            // inverse: unnormalized  -> N × reference
            return inverse ? static_cast<double>(size) : 1.0;
        case fft_scaling::sqrt_n:
            // forward: × sqrt(1/N)   -> reference × sqrt(1/N)
            // inverse: × sqrt(1/N) applied to unnormalized spectrum
            //          = sqrt(N) × reference
            return inverse ? std::sqrt(static_cast<double>(size))
                           : 1.0 / std::sqrt(static_cast<double>(size));
        case fft_scaling::inverse_n:
            // forward: unnormalized  -> matches reference (×1)
            // inverse: × 1/N         -> matches reference (×1)
            return 1.0;
        case fft_scaling::vdsp:
            // forward: 2 × DFT sum   -> reference × 2
            // inverse: unnormalized  -> N × reference
            return inverse ? static_cast<double>(size) : 2.0;
        }
        return 1.0;
    }

    // Prepare the input buffer for an accuracy test.
    //  - complex transforms: copy refin verbatim (interleaved complex).
    //  - real forward (r2c): copy refin verbatim (N real samples).
    //  - real inverse (c2r): convert the ccs reference spectrum in `refin`
    //    into the library's preferred packed layout.
    static void prepare_input(real* dst, const double* refin, size_t in_size, size_t size, bool is_complex,
                              bool inverse, real_layout layout)
    {
        if (is_complex || !inverse)
        {
            for (size_t i = 0; i < in_size; i++)
                dst[i] = static_cast<real>(refin[i]);
            return;
        }
        // c2r: convert ccs reference spectrum → library layout, in place.
        convert_ccs_to_layout(refin, size, layout, dst);
    }

    // Evaluate a library output buffer against the (unscaled) reference.
    // `ref_scale` adapts the reference to the library's normalization.
    //  - complex transforms & real inverse (c2r): direct RMS/max over the
    //    interleaved / time-domain buffer (reference scaled element-wise).
    //  - real forward (r2c): bin-wise comparison of the packed spectrum
    //    against the ccs reference (scaling applied inside compare_spectrum).
    static std::pair<double, double> evaluate(const real* data, const double* refout, size_t out_size,
                                              size_t size, bool is_complex, bool inverse, real_layout layout,
                                              double ref_scale)
    {
        if (is_complex || inverse)
        {
            double sum = 0, maxerr = 0;
            for (size_t i = 0; i < out_size; i++)
            {
                double diff = static_cast<double>(data[i]) - refout[i] * ref_scale;
                sum += diff * diff;
                maxerr = std::max(maxerr, std::abs(diff));
            }
            return { std::sqrt(sum / out_size), maxerr };
        }
        // r2c forward: compare packed spectrum against ccs reference, bin-wise.
        spectrum_error e = compare_spectrum(data, size, layout, refout, ref_scale);
        return { e.rms, e.max };
    }

    template <bool is_complex, bool inverse, bool inplace>
    static unsigned accuracy_test(size_t size, const double* refout, const double* refin, bool progress)
    {
        std::unique_ptr<fft_impl<real>> fft = fft_create<real>({ size }, is_complex, inverse, inplace);
        if (!fft)
        {
            if (progress)
                printf(
                    "%6s accuracy: %31s for %7s %7s %10s DFT of size %zu -- Not supported in the library\n",
                    type_name<real>, "-", is_complex ? "complex" : "real", inverse ? "inverse" : "forward",
                    inplace ? "inplace" : "outofplace", size);
            return 0; // Not a failure of accuracy, just not supported.
        }

        // For real transforms, the library produces/consumes its preferred
        // packed layout (fft->layout()). The reference vectors are always ccs.
        // We adapt on the accuracy-check side: convert the ccs reference into
        // the library's layout for c2r input, and compare r2c output in a
        // layout-agnostic way. No conversion is done inside the wrapper.
        const real_layout layout = is_complex ? real_layout::ccs : fft->layout();
        // The library reports its normalization convention; we scale the
        // reference to match instead of rescaling the library output.
        const double ref_scale = reference_scale(fft->scaling(), inverse, size);

        const size_t spectrum_size = real_spectrum_size(size, layout);
        const size_t in_size       = is_complex ? size * 2 : !inverse ? size : spectrum_size;
        const size_t out_size      = is_complex ? size * 2 : !inverse ? spectrum_size : size;

        double err, maxerr = 0;
        if constexpr (inplace)
        {
            std::vector<real, aligned_allocator<real>> inout(std::max(in_size, out_size));
            inout.reserve(size * 2 + 2); // For safety
            prepare_input(inout.data(), refin, in_size, size, is_complex, inverse, layout);
            fft->execute(inout.data(), inout.data());
            std::tie(err, maxerr) =
                evaluate(inout.data(), refout, out_size, size, is_complex, inverse, layout, ref_scale);
        }
        else
        {
            std::vector<real, aligned_allocator<real>> in(in_size);
            std::vector<real, aligned_allocator<real>> out(out_size);
            in.reserve(size * 2 + 2); // For safety
            out.reserve(size * 2 + 2); // For safety
            prepare_input(in.data(), refin, in_size, size, is_complex, inverse, layout);
            fft->execute(out.data(), in.data());
            std::tie(err, maxerr) =
                evaluate(out.data(), refout, out_size, size, is_complex, inverse, layout, ref_scale);
        }
        if (progress)
            printf("%6s accuracy: %12g (max %12g) for %7s %7s %10s DFT of size %zu\n", type_name<real>, err,
                   maxerr, is_complex ? "complex" : "real", inverse ? "inverse" : "forward",
                   inplace ? "inplace" : "outofplace", size);

        json_open_object();
        json_key("type");
        json_string(type_name<real>);
        json_key("size");
        json_number(size);
        json_key("complex");
        json_bool(is_complex);
        json_key("inverse");
        json_bool(inverse);
        json_key("inplace");
        json_bool(inplace);
        json_key("log_error");
        json_number(std::log10(err));
        json_close_object();
        if constexpr (std::is_same<real, float>::value)
            return maxerr > 1e-3 ? 1 : 0;
        else
            return maxerr > 1e-12 ? 1 : 0;
    }

    template <bool is_complex, bool inverse>
    static unsigned accuracy_test(size_t size, const double* refout, const double* refin, bool progress)
    {
        return accuracy_test<is_complex, inverse, false>(size, refout, refin, progress) +
               accuracy_test<is_complex, inverse, true>(size, refout, refin, progress);
    }

    template <bool is_complex>
    static unsigned accuracy_test(size_t size, const double* refout, const double* refin, bool progress)
    {
        return accuracy_test<is_complex, false>(size, refout, refin, progress) +
               accuracy_test<is_complex, true>(size, refin, refout, progress);
    }

    static unsigned accuracy_tests(bool progress)
    {
        unsigned failed = 0;
        failed += accuracy_test<true>(60, dft_testvector_complex_output60, dft_testvector_complex_input60,
                                      progress);
        failed += accuracy_test<true>(61, dft_testvector_complex_output61, dft_testvector_complex_input61,
                                      progress);
        failed += accuracy_test<true>(62, dft_testvector_complex_output62, dft_testvector_complex_input62,
                                      progress);
        failed += accuracy_test<true>(64, dft_testvector_complex_output64, dft_testvector_complex_input64,
                                      progress);

        failed +=
            accuracy_test<false>(60, dft_testvector_real_output60, dft_testvector_real_input60, progress);
        failed +=
            accuracy_test<false>(61, dft_testvector_real_output61, dft_testvector_real_input61, progress);
        failed +=
            accuracy_test<false>(62, dft_testvector_real_output62, dft_testvector_real_input62, progress);
        failed +=
            accuracy_test<false>(64, dft_testvector_real_output64, dft_testvector_real_input64, progress);
        return failed;
    }

    static unsigned get_cache_level(size_t size, bool is_complex, bool inplace)
    {
        // Query the CPU/OS once; falls back to typical values when unavailable.
        static const cpu_caches caches = get_cpu_caches();
        if (is_complex)
            size *= 2; // complex numbers are 2x larger
        if (!inplace)
            size *= 2; // out-of-place requires 2x memory
        if (size * sizeof(real) <= caches.l1_size)
            return 1;
        if (size * sizeof(real) <= caches.l2_size)
            return 2;
        if (size * sizeof(real) <= caches.l3_size)
            return 3;
        return 4; // larger than L3 cache
    }

    static void benchmark(std::vector<size_t> sizes, bool is_complex, bool inverse, bool inplace,
                          bool progress)
    {
        using namespace std::chrono_literals;

        size_t size          = product(sizes);
        unsigned cache_level = get_cache_level(size, is_complex, inplace);
        if (inplace && cache_level < 2)
        {
            return; // In-place transforms are not supported for small sizes in this benchmark
        }

        json_open_object();
        json_key("size");
        if (sizes.size() == 1)
        {
            json_number(sizes[0]);
        }
        else
        {
            json_open_array();
            for (size_t s : sizes)
                json_number(s);
            json_close_array();
        }
        json_key("data");
        json_string(type_name<real>);
        json_key("type");
        json_string(is_complex_str(is_complex));
        json_key("direction");
        json_string(inverse_str(inverse));
        json_key("buffer");
        json_string(inplace_str(inplace));

        std::unique_ptr<fft_impl<real>> fft = fft_create<real>(sizes, is_complex, inverse, inplace);

        if (!fft)
        {
            json_key("error");
            json_string("Not supported in the library or benchmark");
            json_close_object();
            if (progress)
            {
                printf("%-6s %-7s %-9s %-10s %11s -- Not supported in the library\n", type_name<real>,
                       is_complex_str(is_complex), inverse_str(inverse), inplace_str(inplace),
                       sizes_to_string(sizes).c_str());
            }
            return;
        }

        size_t allocation_size = size * 4 + 2;
        real* in               = aligned_malloc<real>(allocation_size);
        real* out              = aligned_malloc<real>(allocation_size);
        size_t real_in_size    = is_complex ? size * 2
                                 : !inverse ? size
                                            : real_spectrum_size(size, fft->layout());
        std::chrono::nanoseconds total_duration(0);
        uint64_t total_calls = 0;
        std::vector<double> batch_times; // per-call time (seconds) for each batch

        std::chrono::steady_clock::time_point estimate_start = std::chrono::steady_clock::now();
        std::chrono::nanoseconds estimate_duration(0);
        unsigned estimate_calls = 0;
        do
        {
            prng::generate<true>(in, real_in_size);
            estimate_duration += batch(fft.get(), out, in, 1, inplace, real_in_size);
            estimate_calls++;
        } while (std::chrono::steady_clock::now() - estimate_start < std::chrono::duration<double>(0.005));
        std::chrono::duration<double> est_call_time = estimate_duration / estimate_calls;
        unsigned batch_size =
            cache_level >= 2 ? 1 : static_cast<unsigned>(std::max(1.0, ceil(50us / est_call_time)));
        unsigned num_batches = static_cast<unsigned>(std::max(ceil(50ms / est_call_time) / batch_size, 10.0));

        Percentiles percentiles;
        unsigned repeated = 0;
        for (;;)
        {
            ++repeated;
            const unsigned extra_batches = num_batches;

            unsigned b = 0;
            for (; b < num_batches + extra_batches; ++b)
            {
                if (b >= num_batches)
                {
                    auto stat = get_percentiles<true>(batch_times);
                    // Stop once the batch times are tightly clustered.
                    if (stat.p75 / stat.p25 < 1.02)
                    {
                        break;
                    }
                }

                // Create a new FFT object each time to randomize any internal state
                fft = fft_create<real>(sizes, is_complex, inverse, inplace);
                prng::generate<true>(in, real_in_size);
                // Prewarm the FFT object and buffers with a single execute
                batch(fft.get(), out, in, 1, inplace, real_in_size);
                std::chrono::nanoseconds batch_duration =
                    batch(fft.get(), out, in, batch_size, inplace, real_in_size);

                total_duration += batch_duration;
                total_calls += batch_size;

                double duration_s = std::chrono::duration<double>(batch_duration).count() / batch_size;
                batch_times.insert(std::upper_bound(batch_times.begin(), batch_times.end(), duration_s),
                                   duration_s);
            }

            // batch_times is kept sorted on insertion, so skip the redundant sort.
            percentiles = get_percentiles<true>(batch_times);
            if (percentiles.p75 / percentiles.p25 < 1.1)
                break; // Stop once the batch times are tightly clustered.
            if (repeated > 2)
                break; // Stop after 2 repetitions, even if not tightly clustered.

            num_batches *= 2;
        }

        // Choose the reporting statistic based on where the working set fits,
        // giving a smoother transition from compute-bound to memory-bound:
        //   L1-resident (cache_level 1): p1   (best case, compute-bound)
        //   L2-resident (cache_level 2): p25  (slight memory pressure)
        //   L3/RAM       (cache_level 3+): p50 (memory-bound, typical case)
        double time_value = cache_level == 1   ? percentiles.p1
                            : cache_level == 2 ? percentiles.p25
                                               : percentiles.p50_median;
        // FFTW convention: complex FFT costs 5*N*log2(N) FLOP, real FFT half that.
        const double gflops =
            ((is_complex ? 5.0 : 2.5) * size * std::log2((double)size) / time_value) / 1'000'000'000.0;

        if (progress)
        {
            char unit    = percentiles.p1 < 1e-6 ? 'n' : percentiles.p1 < 1e-3 ? 'u' : 'm';
            double scale = unit == 'n' ? 1e9 : unit == 'u' ? 1e6 : 1e3;
            printf("%-6s %-7s %-9s %-10s %11s %5u %10.3f   %10.2f%cs   %10.2f%cs   %10.2f%cs   %7u %8" PRIu64
                   "   %8.4f   %3u\n",
                   type_name<real>, is_complex_str(is_complex), inverse_str(inverse), inplace_str(inplace),
                   sizes_to_string(sizes).c_str(), cache_level, gflops, percentiles.p1 * scale, unit,
                   percentiles.p25 * scale, unit, percentiles.p50_median * scale, unit, batch_size,
                   total_calls, percentiles.p75 / percentiles.p25, repeated);
        }

        json_key("gflops");
        json_number(gflops);
        // Which percentile gflops is derived from: "p1" (compute-bound, fits L1),
        // "p25" (L2-resident) or "p50" (memory-bound). Both raw times are always
        // reported below.
        json_key("gflops_metric");
        json_string(cache_level == 1 ? "p1" : cache_level == 2 ? "p25" : "p50");
        json_key("best_time");
        json_number(percentiles.p1 * 1'000'000);
        json_key("p25_time");
        json_number(percentiles.p25 * 1'000'000);
        json_key("median_time");
        json_number(percentiles.p50_median * 1'000'000);
        json_key("cache_level");
        json_number(cache_level);
        json_close_object();

        if (progress)
        {
            fflush(stdout);
        }
        aligned_free(in, allocation_size);
        aligned_free(out, allocation_size);
    }
};

static std::string outname;
static bool progress = true;
static bool banner   = true;
static bool accuracy = false;
static bool prewarm  = false;
static std::vector<std::vector<size_t>> sizes;
static std::vector<bool> is_double_list{ false, true };
static std::vector<bool> is_complex_list{ true };
static std::vector<bool> inverse_list{ false };
static std::vector<bool> inplace_list{ true };

template <typename real>
static void run_t(const std::vector<size_t>& sizes)
{
    for (bool complex : is_complex_list)
    {
        for (bool inverse : inverse_list)
        {
            for (bool inplace : inplace_list)
            {
                fft_benchmark_runner<real>::benchmark(sizes, complex, inverse, inplace, progress);
            }
        }
    }
}

static void run(const std::vector<size_t>& sizes)
{
    if (std::find(is_double_list.begin(), is_double_list.end(), false) != is_double_list.end())
        run_t<float>(sizes);
    if (std::find(is_double_list.begin(), is_double_list.end(), true) != is_double_list.end())
        run_t<double>(sizes);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    using namespace std::string_view_literals;

    // Expand @<arguments_file> into the argument list. The --save argument is
    // never read from file (it must be passed on the command line).
    std::vector<std::string> expanded_args;
    for (size_t i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg.size() > 1 && arg[0] == '@')
        {
            std::string filename = arg.substr(1);
            std::ifstream f(filename);
            if (!f)
            {
                fprintf(stderr, "Cannot open arguments file: %s\n", filename.c_str());
                return 1;
            }
            std::string token;
            bool skip_next = false;
            while (f >> token)
            {
                if (skip_next)
                {
                    // This token is the value of a --save read from file; drop it.
                    skip_next = false;
                    continue;
                }
                if (token == "--save")
                {
                    // --save (and its value) must come from the command line, not a file.
                    skip_next = true;
                    continue;
                }
                expanded_args.push_back(token);
            }
        }
        else
        {
            expanded_args.push_back(arg);
        }
    }

    for (size_t i = 0; i < expanded_args.size(); i++)
    {
        const std::string& arg = expanded_args[i];
        if (arg == "--save"sv)
        {
            if (i + 1 < expanded_args.size())
            {
                outname = expanded_args[i + 1];
                ++i;
            }
        }
        else if (arg == "--no-progress"sv)
        {
            progress = false;
        }
        else if (arg == "--progress"sv)
        {
            progress = true;
        }
        else if (arg == "--no-accuracy"sv)
        {
            accuracy = false;
        }
        else if (arg == "--accuracy"sv)
        {
            accuracy = true;
        }
        else if (arg == "--no-prewarm"sv)
        {
            prewarm = false;
        }
        else if (arg == "--prewarm"sv)
        {
            prewarm = true;
        }
        else if (arg == "--no-banner"sv)
        {
            banner = false;
        }
        else if (arg == "--banner"sv)
        {
            banner = true;
        }
        else if (arg == "--double"sv)
        {
            if (i + 1 < expanded_args.size())
            {
                is_double_list = to_vector_bool(expanded_args[i + 1]);
                ++i;
            }
        }
        else if (arg == "--complex"sv)
        {
            if (i + 1 < expanded_args.size())
            {
                is_complex_list = to_vector_bool(expanded_args[i + 1]);
                ++i;
            }
        }
        else if (arg == "--inplace"sv)
        {
            if (i + 1 < expanded_args.size())
            {
                inplace_list = to_vector_bool(expanded_args[i + 1]);
                ++i;
            }
        }
        else if (arg == "--inverse"sv)
        {
            if (i + 1 < expanded_args.size())
            {
                inverse_list = to_vector_bool(expanded_args[i + 1]);
                ++i;
            }
        }
        else if (arg == "--"sv)
        {
        }
        else
        {
            auto size = parse_size(arg);
            if (size.empty())
            {
                fprintf(stderr, "Incorrect size: %s\n", arg.c_str());
                return 1;
            }
            sizes.push_back(std::move(size));
        }
    }

    std::string cpuname = cpu_name();
    std::string fftname = fft_name();

    const auto caches = get_cpu_caches();

    if (banner)
    {
        printf("FFT/DFT benchmarking tool. Copyright (C) 2016-2026 Dan Casarin https://www.kfrlib.com\n");
        printf("Benchmark source code is MIT-licensed\n");
        printf("DFT/FFT libraries have their own licenses. Please refer to the respective source code for "
               "details.\n");

        if (sizes.empty())
        {
            printf("Usage:\n");
            printf("        %s [--save filename] [--no-progress] [--no-banner] <size> <size> ... <size>\n",
                   execfile(argv[0]).c_str());
            printf("Example:\n");
            printf("        %s --save results.json 262144 512x512 64x64x64 # run and save to json\n\n",
                   execfile(argv[0]).c_str());
            printf("        %s --save - 262144 512x512 64x64x64 # run and print to stdout\n\n",
                   execfile(argv[0]).c_str());
        }
        printf("CPU: %s\n", cpuname.c_str());
        printf("CPU caches: %s\n", cpu_caches_string(caches).c_str());
        printf("Algorithm: %s\n", fftname.c_str());
        printf("Compiler: %s %s\n", CMAKE_CXX_COMPILER_ID, CMAKE_CXX_COMPILER_VERSION);
        printf("Benchmark git commit: %s\n", GIT_COMMIT_HASH);
    }

    if (progress)
    {
        printf("calibrating tsc...");
        fflush(stdout);
    }
    {
        benchmark_scope scope; // run at benchmarking priority/frequency
        details::calibrate_tsc();
    }

    if (progress)
    {
        printf(" %.2fMHz\n", 1000.0 / tsc_resolution());
    }

    json_open_object();

    json_key("cpu");
    json_string(cpuname);

    json_key("cpu_caches");
    json_open_object();
    json_key("l1");
    json_number(caches.l1_size);
    json_key("l2");
    json_number(caches.l2_size);
    json_key("l3");
    json_number(caches.l3_size);
    json_key("line_size");
    json_number(caches.line_size);
    json_close_object();

    json_key("clock_MHz");
    json_number(1000.0 / tsc_resolution());

    json_key("library");
    json_string(fftname);

    json_key("git_commit");
    json_string(GIT_COMMIT_HASH);

    unsigned accuracy_failed = 0;
    if (accuracy)
    {
        json_key("accuracy");
        json_open_array();
        accuracy_failed += fft_benchmark_runner<float>::accuracy_tests(progress);
        accuracy_failed += fft_benchmark_runner<double>::accuracy_tests(progress);
        json_close_array();
    }
    if (accuracy_failed == 0)
    {
        json_key("performance");
        json_open_array();

        if (sizes.empty())
        {
            fprintf(stderr, "No sizes specified\n");
            return accuracy ? 0 : 1;
        }

        if (progress)
        {
            printf("%-6s %-7s %-9s %-10s %11s %5s %10s   %12s   %12s   %12s   %7s %8s   %8s   %3s\n", "data",
                   "type", "direction", "buffer", "size", "cache", "gflops", "P1", "P25", "P50", "batch",
                   "calls", "P75/P25", "R");
        }

        benchmark_scope scope;
        if (prewarm)
        {
            printf("Warming up the CPU with synthetic workload...");
            synthetic_workload(10'000'000'000);
            printf("ok\n");
        }

        for (auto size : sizes)
        {
            if (size.size() < 1 || size.size() > 3)
            {
                fprintf(stderr, "Incorrect number of dimensions: %zu\n", size.size());
                return 1;
            }
            else
            {
                run(size);
            }
        }

        json_close_array();
    }

    json_close_object();

    if (outname.empty())
        return accuracy_failed ? 1 : 0;
    if (outname != "-")
    {
        freopen(outname.c_str(), "w", stdout);
    }
    fputs(json_output.c_str(), stdout);
    return accuracy_failed ? 1 : 0;
}
