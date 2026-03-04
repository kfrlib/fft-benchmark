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

template <typename real>
struct fft_benchmark_runner
{
    constexpr static int preheat_calls = 5;
    constexpr static int calls_per_run = 10;

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
                          bool progress, uint32_t benchmark_duration = 2500)
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

        size_t size = product(sizes);
        real* in    = aligned_malloc<real>(size * 4 + 2);
        real* out   = aligned_malloc<real>(size * 4 + 2);
        fill_random(in, size * 2);
        if (inplace)
            std::copy(in, in + size * 2, out);
        std::chrono::nanoseconds minimum_duration(std::chrono::seconds(1000));
        std::chrono::nanoseconds total_duration(0);
        uint64_t total_calls = 0;
        std::vector<double> batch_times; // per-call time (seconds) for each batch

        {
            benchmark_scope scope;
            for (;;)
            {
                for (int i = 0; i < preheat_calls; ++i)
                {
                    fft->execute(out, inplace ? out : in);
                    dont_optimize(out);
                }
                bench_start();
                for (int i = 0; i < calls_per_run; ++i)
                {
                    fft->execute(out, inplace ? out : in);
                    dont_optimize(out);
                }
                auto run_duration = bench_stop();
                total_duration += run_duration;
                minimum_duration = std::min(minimum_duration, run_duration);
                total_calls += calls_per_run;
                batch_times.push_back(std::chrono::duration<double>(run_duration).count() / calls_per_run);

                fill_random(in, size * 2);
                if (inplace)
                    std::copy(in, in + size * 2, out);

                if ((total_duration >= std::chrono::milliseconds(benchmark_duration) && total_calls >= 50) ||
                    total_calls >= 1000'000)
                    break;
            }
        } // benchmark_scope

        [[maybe_unused]] double median_time = get_median(batch_times);
        [[maybe_unused]] double minimum_time =
            std::chrono::duration<double>(minimum_duration).count() / calls_per_run;
        [[maybe_unused]] double opspersecond_median = 1.0 / median_time;
        [[maybe_unused]] double opspersecond_best   = 1.0 / minimum_time;

        double time_value = minimum_time;
        // FFTW convention: complex FFT costs 5*N*log2(N) FLOP, real FFT half that.
        const double mflops =
            ((is_complex ? 5.0 : 2.5) * size * std::log((double)size) / (std::log(2.0) * time_value)) /
            1000'000.0;

        if (progress)
        {
            printf("%-6s %-7s %-9s %-10s %11s %12.2f | %12.2fus%12.2f | %12.2fus%12.2f | %7" PRIu64 "\n",
                   type_name<real>, is_complex_str(is_complex), inverse_str(inverse), inplace_str(inplace),
                   sizes_to_string(sizes).c_str(), mflops, minimum_time * 1'000'000, opspersecond_best,
                   median_time * 1'000'000, opspersecond_median, total_calls);
        }

        json_key("mflops");
        json_number(mflops);
        json_key("best_time");
        json_number(minimum_time * 1'000'000);
        json_key("median_time");
        json_number(median_time * 1'000'000);
        json_close_object();

        if (progress)
        {
            fflush(stdout);
        }
        aligned_free(in);
        aligned_free(out);
    }
};

static std::string outname;
static bool progress = true;
static bool banner   = true;
static bool accuracy = true;
bool avx2only        = false;
static std::vector<std::vector<size_t>> sizes;
static std::vector<bool> is_complex_list{ true, false };
static std::vector<bool> inverse_list{ false, true };
static std::vector<bool> inplace_list{ false, true };

template <typename real>
static void run_t(const std::vector<size_t>& sizes, bool progress)
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

static void run(const std::vector<size_t>& sizes, bool progress)
{
    run_t<float>(sizes, progress);
    run_t<double>(sizes, progress);
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
        printf(" %.1fMHz\n", 1000.0 / tsc_resolution());
    }

    json_open_object();

    if (progress)
    {

        printf("%-6s %-7s %-9s %-10s %11s %12s | %14s%12s | %14s%12s | %7s\n", "data", "type", "direction",
               "buffer", "size", "mflops", "best time", "(ops/sec)", "med. time", "(ops/sec)", "calls");
    }

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

    for (auto size : sizes)
    {
        if (size.size() < 1 || size.size() > 3)
        {
            fprintf(stderr, "Incorrect number of dimensions: %zu\n", size.size());
            return 1;
        }
        else
        {
            run(size, progress);
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
