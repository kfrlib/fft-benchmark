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

#include "json.hpp"

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

// #define SYNTHETIC_WORKLOAD

#ifdef SYNTHETIC_WORKLOAD

[[clang::noinline]] void synthetic_fma(long long iterations)
{
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
}
#endif

template <typename real>
BENCH_INLINE void preheat_fft(fft_impl<real>* fft, real* out, const real* in, size_t inner_iterations)
{
    for (int i = 0; i < inner_iterations; ++i)
    {
        fft->execute(out, in);
        dont_optimize(out);
    }
}

template <typename real>
BENCH_INLINE std::chrono::nanoseconds measure_fft(fft_impl<real>* fft, real* out, const real* in,
                                                  size_t fft_size, size_t inner_iterations)
{
#ifdef SYNTHETIC_WORKLOAD
    uint64_t iter = std::log2(fft_size) * fft_size / 10;
#endif
    bench_start();
    for (int i = 0; i < inner_iterations; ++i)
    {
#ifdef SYNTHETIC_WORKLOAD
        synthetic_fma(iter);
#else
        fft->execute(out, in);
#endif
        dont_optimize(out);
    }
    return bench_stop();
}

template <typename real>
struct fft_benchmark_runner
{

    static double compute_max_error(const real* data, const double* refout, size_t out_size)
    {
        double maxerr = 0;
        for (size_t i = 0; i < out_size; i++)
            maxerr = std::max(maxerr, std::abs(data[i] - refout[i]));
        return maxerr;
    }

    static void maybe_rescale_inverse(std::vector<real, aligned_allocator<real>>& data, size_t out_size,
                                      size_t size, bool inverse, double& err, const double* refout)
    {
        if (err > 1e-4 && inverse)
        {
            double scale = 1.0 / size;
            for (size_t i = 0; i < out_size; i++)
                data[i] *= static_cast<real>(scale);
            err = rms(data.data(), refout, out_size);
        }
    }

    template <bool is_complex, bool inverse, bool inplace>
    static double accuracy_test(size_t size, const double* refout, const double* refin, bool progress)
    {
        std::unique_ptr<fft_impl<real>> fft = fft_create<real>({ size }, is_complex, inverse, inplace);
        if (!fft)
        {
            if (progress)
                printf(
                    "%6s accuracy: %31s for %7s %7s %10s DFT of size %zu -- Not supported in the library\n",
                    type_name<real>, "-", is_complex ? "complex" : "real", inverse ? "inverse" : "forward",
                    inplace ? "inplace" : "outofplace", size);
            return -1;
        }
        const size_t in_size  = is_complex ? size * 2 : !inverse ? size : size / 2 * 2 + 2;
        const size_t out_size = is_complex ? size * 2 : !inverse ? size / 2 * 2 + 2 : size;
        double err, maxerr = 0;
        if constexpr (inplace)
        {
            std::vector<real, aligned_allocator<real>> inout(std::max(in_size, out_size));
            inout.reserve(size * 2 + 2); // For safety
            std::copy(refin, refin + in_size, inout.data());
            fft->execute(inout.data(), inout.data());
            err = rms(inout.data(), refout, out_size);
            maybe_rescale_inverse(inout, out_size, size, inverse, err, refout);
            maxerr = compute_max_error(inout.data(), refout, out_size);
        }
        else
        {
            std::vector<real, aligned_allocator<real>> in(in_size);
            std::vector<real, aligned_allocator<real>> out(out_size);
            in.reserve(size * 2 + 2); // For safety
            out.reserve(size * 2 + 2); // For safety
            std::copy(refin, refin + in_size, in.data());
            fft->execute(out.data(), in.data());
            err = rms(out.data(), refout, out_size);
            maybe_rescale_inverse(out, out_size, size, inverse, err, refout);
            maxerr = compute_max_error(out.data(), refout, out_size);
        }
        if (progress)
            printf("%6s accuracy: %12g (max %12g) for %7s %7s %10s DFT of size %zu\n", type_name<real>, err,
                   maxerr, is_complex ? "complex" : "real", inverse ? "inverse" : "forward",
                   inplace ? "inplace" : "outofplace", size);
        return err;
    }

    template <bool is_complex, bool inverse>
    static double accuracy_test(size_t size, const double* refout, const double* refin, bool progress)
    {
        double err = accuracy_test<is_complex, inverse, false>(size, refout, refin, progress);
        err        = std::max(err, accuracy_test<is_complex, inverse, true>(size, refout, refin, progress));
        return err;
    }

    template <bool is_complex>
    static void accuracy_test(size_t size, const double* refout, const double* refin, bool progress)
    {
        double err = accuracy_test<is_complex, false>(size, refout, refin, progress);
        err        = std::max(err, accuracy_test<is_complex, true>(size, refin, refout, progress));
        if (err < 0)
            return;
        json_open_object();
        json_key("type");
        json_string(type_name<real>);
        json_key("size");
        json_number(size);
        json_key("log_error");
        json_number(std::log10(err));
        json_close_object();
    }

    static void accuracy_tests(bool progress)
    {
        accuracy_test<true>(60, dft_testvector_complex_output60, dft_testvector_complex_input60, progress);
        accuracy_test<true>(61, dft_testvector_complex_output61, dft_testvector_complex_input61, progress);
        accuracy_test<true>(62, dft_testvector_complex_output62, dft_testvector_complex_input62, progress);
        accuracy_test<true>(64, dft_testvector_complex_output64, dft_testvector_complex_input64, progress);

        accuracy_test<false>(60, dft_testvector_real_output60, dft_testvector_real_input60, progress);
        accuracy_test<false>(61, dft_testvector_real_output61, dft_testvector_real_input61, progress);
        accuracy_test<false>(62, dft_testvector_real_output62, dft_testvector_real_input62, progress);
        accuracy_test<false>(64, dft_testvector_real_output64, dft_testvector_real_input64, progress);
    }

    static void benchmark(std::vector<size_t> sizes, bool is_complex, bool inverse, bool inplace,
                          bool progress, double benchmark_duration = 2)
    {
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

        size_t size            = product(sizes);
        size_t allocation_size = size * 4 + 2;
        real* in               = aligned_malloc<real>(allocation_size);
        real* out              = aligned_malloc<real>(allocation_size);
        fill_random(in, size * 2);
        if (inplace)
            std::copy(in, in + size * 2, out);
        std::chrono::nanoseconds total_duration(0);
        uint64_t total_calls = 0;
        std::vector<double> batch_times; // per-call time (seconds) for each batch

        size_t inner_iterations = size <= 256 ? 100 : size <= 262144 ? 10 : 1;

        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time <
               std::chrono::duration<double>(benchmark_duration / 4))
        {
            preheat_fft(fft.get(), out, inplace ? out : in, inner_iterations);
        }

        for (;;)
        {
            std::chrono::nanoseconds run_duration;
            run_duration = measure_fft<real>(fft.get(), out, inplace ? out : in, size, inner_iterations);

            total_duration += run_duration;
            total_calls += inner_iterations;
            batch_times.push_back(std::chrono::duration<double>(run_duration).count() / inner_iterations);

            fill_random(in, size * 2);
            if (inplace)
                std::copy(in, in + size * 2, out);

            if (total_calls / inner_iterations >= 10)
            {
                if (total_duration >= std::chrono::duration<double>(benchmark_duration))
                    break;
            }
        }

        [[maybe_unused]] auto times                 = get_percentiles(batch_times);
        [[maybe_unused]] double opspersecond_median = 1.0 / times.p50_median;
        [[maybe_unused]] double opspersecond_best   = 1.0 / times.p1;

        double time_value = times.p1;
        // FFTW convention: complex FFT costs 5*N*log2(N) FLOP, real FFT half that.
        const double gflops =
            ((is_complex ? 5.0 : 2.5) * size * std::log((double)size) / (std::log(2.0) * time_value)) /
            1'000'000'000.0;

        if (progress)
        {
            char unit    = times.p1 < 1e-6 ? 'n' : 'u';
            double scale = unit == 'n' ? 1e9 : 1e6;
            printf("%-6s %-7s %-9s %-10s %11s %10.3f | %12.2f%cs%12.2f | %12.2f%cs%12.2f | %7" PRIu64 "\n",
                   type_name<real>, is_complex_str(is_complex), inverse_str(inverse), inplace_str(inplace),
                   sizes_to_string(sizes).c_str(), gflops, times.p1 * scale, unit, opspersecond_best,
                   times.p50_median * scale, unit, opspersecond_median, total_calls);
        }

        json_key("gflops");
        json_number(gflops);
        json_key("best_time");
        json_number(times.p1 * 1'000'000);
        json_key("median_time");
        json_number(times.p50_median * 1'000'000);
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
static bool progress   = true;
static bool banner     = true;
static bool accuracy   = false;
static double duration = 2.0;
bool avx2only          = false;
static std::vector<std::vector<size_t>> sizes;
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
                fft_benchmark_runner<real>::benchmark(sizes, complex, inverse, inplace, progress, duration);
            }
        }
    }
}

static void run(const std::vector<size_t>& sizes)
{
    benchmark_scope scope;
    run_t<float>(sizes);
    run_t<double>(sizes);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    using namespace std::string_view_literals;

    for (size_t i = 1; i < argc; i++)
    {
        if (argv[i] == "--save"sv)
        {
            if (i + 1 < argc)
            {
                outname = argv[i + 1];
                ++i;
            }
        }
        else if (argv[i] == "--no-progress"sv)
        {
            progress = false;
        }
        else if (argv[i] == "--progress"sv)
        {
            progress = true;
        }
        else if (argv[i] == "--no-accuracy"sv)
        {
            accuracy = false;
        }
        else if (argv[i] == "--accuracy"sv)
        {
            accuracy = true;
        }
        else if (argv[i] == "--no-banner"sv)
        {
            banner = false;
        }
        else if (argv[i] == "--banner"sv)
        {
            banner = true;
        }
        else if (argv[i] == "--complex"sv)
        {
            if (i + 1 < argc)
            {
                is_complex_list = to_vector_bool(argv[i + 1]);
                ++i;
            }
        }
        else if (argv[i] == "--inplace"sv)
        {
            if (i + 1 < argc)
            {
                inplace_list = to_vector_bool(argv[i + 1]);
                ++i;
            }
        }
        else if (argv[i] == "--inverse"sv)
        {
            if (i + 1 < argc)
            {
                inverse_list = to_vector_bool(argv[i + 1]);
                ++i;
            }
        }
        else if (argv[i] == "--duration"sv)
        {
            if (i + 1 < argc)
            {
                duration = std::stod(argv[i + 1]);
                ++i;
            }
        }
        else if (argv[i] == "--avx2-only"sv)
        {
            avx2only = true;
        }
        else if (argv[i] == "--"sv)
        {
        }
        else
        {
            auto size = parse_size(argv[i]);
            if (size.empty())
            {
                fprintf(stderr, "Incorrect size: %s\n", argv[i]);
                return 1;
            }
            sizes.push_back(std::move(size));
        }
    }

    std::string cpuname = cpu_name();
    std::string fftname = fft_name();

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
        printf("Algorithm: %s\n", fftname.c_str());
        printf("Compiler: %s %s\n", CMAKE_CXX_COMPILER_ID, CMAKE_CXX_COMPILER_VERSION);
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

    json_key("clock_MHz");
    json_number(1000.0 / tsc_resolution());

    json_key("library");
    json_string(fftname);

    if (accuracy)
    {
        json_key("accuracy");
        json_open_array();
        fft_benchmark_runner<float>::accuracy_tests(progress);
        fft_benchmark_runner<double>::accuracy_tests(progress);
        json_close_array();
    }

    json_key("performance");
    json_open_array();

    if (sizes.empty())
    {
        fprintf(stderr, "No sizes specified\n");
        return 1;
    }

    if (progress)
    {
        printf("%-6s %-7s %-9s %-10s %11s %10s | %14s%12s | %14s%12s | %7s\n", "data", "type", "direction",
               "buffer", "size", "gflops", "best time", "(ops/sec)", "med. time", "(ops/sec)", "calls");
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

    json_close_object();

    if (outname.empty())
        return 0;
    if (outname != "-")
    {
        freopen(outname.c_str(), "w", stdout);
    }
    fputs(json_output.c_str(), stdout);
    return 0;
}
