/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016 D Levin
 * Benchmark source code is dual-licensed under MIT and GPL 2 or later
 * See LICENSE.txt for details
 */

#ifndef __OPTIMIZE__
#error fft_benchmark can't be built in a non-optimized mode
#endif

#include "mingw_fix.h"

#ifdef TYPE_FLOAT
typedef float real;
typedef _Complex float complex;
const char* real_name = "float";
#else
typedef double real;
typedef _Complex double complex;
const char* real_name = "double";
#endif

#include "benchmark.hpp"
#include FFT_TEST
#include <cmath>
#include <string>

void fill_random(real* in, size_t size)
{
    for (size_t i = 0; i < size; i++)
        in[i]     = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
}

void test_fft(size_t size, double time = 1.5)
{
    const tick_value correction = 0;//calibrate_correction();
    printf(">  [%9ld, ", (long)size);
    real* in  = aligned_malloc<real>(size * 2);
    real* out = aligned_malloc<real>(size * 2);
    fill_random(in, size * 2);
    std::copy(in, in + size * 2, out);
    fft_benchmark<false> fft(size, out, out);

    fft.execute();
    fill_random(in, size * 2);

    std::vector<tick_value> measures;
    const tick_value bench_tick_start = tick();
    const time_value bench_start      = now();
    while (time_between(now(), bench_start) < time)
    {
        const tick_value start = tick();
        fft.execute();
        const tick_value stop = tick();
        const tick_value diff = stop - start;
        measures.push_back(diff >= correction ? diff - correction : 0);
        fill_random(in, size * 2);
        dont_optimize(out);
        std::copy(in, in + size * 2, out);
    }
    const tick_value bench_tick_stop = tick();
    const time_value bench_stop      = now();

    const double tick_frequency =
        (long double)(bench_tick_stop - bench_tick_start) / time_between(bench_stop, bench_start);

    tick_value tick_value = get_minimum(measures);
    double time_value     = tick_value / tick_frequency;
    const double flops     = (5.0 * size * std::log((double)size) / (std::log(2.0) * time_value));

    const char* units = "'s' ";
    if (time_value < 0.000001)
    {
        units       = "'ns'";
        time_value = time_value * 1000000000.0;
    }
    else if (time_value < 0.001)
    {
        units       = "'us'";
        time_value = time_value * 1000000.0;
    }
    else if (time_value < 1.0)
    {
        units       = "'ms'";
        time_value = time_value * 1000.0;
    }

    printf("%9lld, ", tick_value);
    printf("%7g, %s, ", time_value, units);
    printf("%7g, ", flops / 1000000.0);
    printf("%8lld, ", measures.size());
    printf("%8.6f],\n", tick_frequency / 1000000000.0);

    aligned_free(in);
    aligned_free(out);
}

std::string execfile(char** argv)
{
    std::string result = argv[0];
    size_t pos         = result.find_last_of("/\\");
    result             = result.substr(pos == std::string::npos ? 0 : pos + 1);
    if (result.substr(result.size() - 4) == ".exe")
        result = result.substr(0, result.size() - 4);
    return result;
}

int main(int argc, char** argv)
{
    set_affinity();
    printf("FFT/DFT benchmarking tool\n");
    printf("Copyright (C) 2016 D Levin https://www.kfrlib.com\n");
    printf("Benchmark source code is dual-licensed under MIT and GPL 2 or later\n");
    printf("Individual DFT/FFT algorithms have its own licenses. See its source code for details\n");
    printf("Usage:\n        %s <size> <size> ... <size>\n\n", execfile(argv).c_str());
    printf("Data type: %s\n", real_name);
    printf("Algorithm: %s\n", fft_benchmark<false>::name().c_str());
    printf("Built using %s\n\n", __VERSION__);

    printf(">[\n");
    for (size_t i = 1; i < argc; i++)
    {
        const long size = std::atol(argv[i]);
        if (size > 1)
            test_fft(size);
        else
            printf("Incorrect size: %s\n", argv[i]);
    }
    printf(">]\n");
    return 0;
}
