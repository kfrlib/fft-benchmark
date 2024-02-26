/**
 * FFT bencmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2023 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#define NOMINMAX 1

#include <cstring>
#include <inttypes.h>

#include "benchmark.hpp"
#include <cmath>
#include <string>

#include "json.hpp"

template <typename real>
void fill_random(real* in, size_t size)
{
    for (size_t i = 0; i < size; i++)
        in[i] = static_cast<real>(((double)rand() / RAND_MAX) * 2.0 - 1.0);
}

template <typename real>
struct benchmark_runner
{
    constexpr static int preheat_calls = 1;
    constexpr static int calls_per_run = 10;

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
        real* in    = aligned_malloc<real>(size * 2 + 2);
        real* out   = aligned_malloc<real>(size * 2 + 2);
        fill_random(in, size * 2);
        if (inplace)
            std::copy(in, in + size * 2, out);
        std::chrono::nanoseconds minimum_duration(std::chrono::seconds(1000));
        std::chrono::nanoseconds total_duration(0);
        uint64_t total_calls = 0;

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

            fill_random(in, size * 2);
            if (inplace)
                std::copy(in, in + size * 2, out);

            if ((total_duration >= std::chrono::milliseconds(benchmark_duration) && total_calls >= 50) ||
                total_calls >= 1000'000)
                break;
        }

        [[maybe_unused]] double average_time =
            std::chrono::duration<double>(total_duration).count() / total_calls;
        [[maybe_unused]] double minimum_time =
            std::chrono::duration<double>(minimum_duration).count() / calls_per_run;
        [[maybe_unused]] double opspersecond_avg  = 1.0 / average_time;
        [[maybe_unused]] double opspersecond_best = 1.0 / minimum_time;

        double time_value = minimum_time;
        const double mflops =
            (5.0 * size * std::log((double)size) / (std::log(2.0) * time_value)) / 1000'000.0;

        if (progress)
        {
            printf("%-6s %-7s %-9s %-10s %11s %12.2f | %12.2fus%12.2f | %12.2fus%12.2f | %7" PRIu64 "\n",
                   type_name<real>, is_complex_str(is_complex), inverse_str(inverse), inplace_str(inplace),
                   sizes_to_string(sizes).c_str(), mflops, minimum_time * 1'000'000, opspersecond_best,
                   average_time * 1'000'000, opspersecond_avg, total_calls);
        }

        json_key("mflops");
        json_number(mflops);
        json_key("best_time");
        json_number(minimum_time * 1'000'000);
        json_key("avg_time");
        json_number(average_time * 1'000'000);
        json_close_object();

        if (progress)
        {
            fflush(stdout);
        }
        aligned_free(in);
        aligned_free(out);
    }
};

std::string execfile(std::string command)
{
    size_t pos = command.find_last_of("/\\");
    command    = command.substr(pos == std::string::npos ? 0 : pos + 1);
    if (command.substr(command.size() - 4) == ".exe")
        command = command.substr(0, command.size() - 4);
    return command;
}

static std::string outname;
static bool progress = true;
static bool banner   = true;
bool avx2only        = false;
static std::vector<std::vector<size_t>> sizes;
static std::vector<bool> is_complex_list{ true, false };
static std::vector<bool> inverse_list{ false, true };
static std::vector<bool> inplace_list{ false, true };

static size_t parse_number(std::string_view& s)
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

static std::vector<size_t> parse_size(std::string_view s)
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

template <typename real>
static void run_t(std::vector<size_t> sizes, bool progress)
{
    for (bool complex : is_complex_list)
    {
        for (bool inverse : inverse_list)
        {
            for (bool inplace : inplace_list)
            {
                benchmark_runner<real>::benchmark(sizes, complex, inverse, inplace, progress);
            }
        }
    }
}

static void run(std::vector<size_t> sizes, bool progress)
{
    run_t<float>(sizes, progress);
    run_t<double>(sizes, progress);
}

using namespace std::string_view_literals;

static std::vector<bool> to_vector_bool(std::string_view s)
{
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

int main(int argc, char** argv)
{
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
        else if (argv[i] == "--no-banner"sv)
        {
            banner = false;
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
        else if (argv[i] == "--banner"sv)
        {
            banner = true;
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
        printf("FFT/DFT benchmarking tool. Copyright (C) 2016-2024 Dan Cazarin https://www.kfrlib.com\n");
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
        details::calibrate_tsc();
        printf(" %.1fMHz\n", 1000.0 / tsc_resolution());

        printf("%-6s %-7s %-9s %-10s %11s %12s | %14s%12s | %14s%12s | %7s\n", "data", "type", "direction",
               "buffer", "size", "mflops", "best time", "(ops/sec)", "avg. time", "(ops/sec)", "calls");
    }

    json_open_object();

    json_key("cpu");
    json_string(cpuname);

    json_key("clock_MHz");
    json_number(1000.0 / tsc_resolution());

    json_key("library");
    json_string(fftname);

    json_key("results");
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
