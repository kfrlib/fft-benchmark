/**
 * FFT bencmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "src_benchmark.hpp"
#include "json.hpp"
#include <cinttypes>

#define NOMINMAX 1

bool avx2only = false;

template <typename real>
static void run_t(unsigned out_rate, unsigned in_rate, unsigned length, bool progress)
{
    src_impl_ptr<real> src(src_create<real>(out_rate, in_rate, length));
    if (!src || !src->valid)
    {
        if (progress)
            fprintf(stderr, "Source implementation is not valid for this configuration\n");
        return;
    }

    json_open_object();
    json_key("data");
    json_string(type_name<real>);
    json_key("out_rate");
    json_number(out_rate);
    json_key("in_rate");
    json_number(in_rate);
    json_key("length_seconds");
    json_number(length);

    // length is in seconds
    size_t in_length  = in_rate * length;
    size_t out_length = out_rate * length;
    std::vector<real, aligned_allocator<real>> in(in_length, real(0));
    std::vector<real, aligned_allocator<real>> out(out_length, real(0));

    constexpr size_t warmup_iterations = 2;
    constexpr size_t iterations        = 10;
    std::vector<double> measures;
    measures.reserve(iterations);

    for (size_t i = 0; i < warmup_iterations; i++)
    {
        src->execute(out.data(), in.data());
    }
    for (size_t i = 0; i < iterations; i++)
    {
        bench_start();
        src->execute(out.data(), in.data());
        std::chrono::nanoseconds time = bench_stop();
        measures.push_back(std::chrono::duration<double>(time).count());
    }
    double median_s       = get_median(measures);
    double minimum_s      = get_minimum(measures);
    double median_factor  = static_cast<double>(length) / median_s;
    double minimum_factor = static_cast<double>(length) / minimum_s;
    if (progress)
    {
        printf("%6s  %12.3f ms %5.1fx %12.3f ms %5.1fx\n", type_name<real>, median_s * 1000.0, median_factor,
               minimum_s * 1000.0, minimum_factor);
    }

    json_key("best_time");
    json_number(minimum_s * 1'000'000);
    json_key("median_time");
    json_number(median_s * 1'000'000);
    json_close_object();
}

static void run(unsigned out_rate, unsigned in_rate, unsigned length, bool progress)
{
    if (progress)
    {
        printf("Benchmarking SRC from %u Hz to %u Hz for %u seconds\n", in_rate, out_rate, length);
        printf("%6s  %15s %6s %15s %6s\n", "Type", "Median Time", "xRT", "Best Time", "xRT");
    }
    run_t<float>(out_rate, in_rate, length, progress);
    run_t<double>(out_rate, in_rate, length, progress);
    if (progress)
    {
        printf("--------------------------------------------------------------\n");
    }
}

static std::string outname;
static bool progress = true;
static bool banner   = true;

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
        else if (argv[i] == "--no-banner"sv)
        {
            banner = false;
        }
        else if (argv[i] == "--avx2-only"sv)
        {
            avx2only = true;
        }
        else if (argv[i] == "--"sv)
        {
        }
    }

    std::string cpuname = cpu_name();
    std::string srcname = src_name();

    if (banner)
    {
        printf("DSP benchmarking tool. Copyright (C) 2016-2026 Dan Casarin https://www.kfrlib.com\n");
        printf("Benchmark source code is MIT-licensed\n");
        printf("DSP libraries have their own licenses. Please refer to the respective source code for "
               "details.\n");

        printf("CPU: %s\n", cpuname.c_str());
        printf("Algorithm: %s\n", srcname.c_str());
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

    json_key("cpu");
    json_string(cpuname);

    json_key("clock_MHz");
    json_number(1000.0 / tsc_resolution());

    json_key("library");
    json_string(srcname);

    json_key("performance");
    json_open_array();

    run(19997, 40009, 60, progress);
    run(40009, 19997, 60, progress);
    run(48000, 44100, 60, progress);
    run(44100, 48000, 60, progress);
    run(96000, 48000, 60, progress);
    run(48000, 96000, 60, progress);

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