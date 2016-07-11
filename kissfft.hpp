/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016 D Levin
 * Benchmark source code is dual-licensed under MIT and GPL 2 or later
 * See LICENSE.txt for details
 */
#pragma once

#include "benchmark.hpp"
#define kiss_fft_scalar real
#include "kissfft/kiss_fft.h"
#include <complex.h>
#include <complex>
#include <string>

template <bool invert>
class fft_benchmark
{
public:
    fft_benchmark(size_t size, real* out, const real* in)
        : plan(kiss_fft_alloc(size, invert, NULL, NULL)), out(out), in(in)
    {
    }
    static std::string name() { return "KissFFT 1.3.0"; }
    static std::string shortname() { return "KissFFT"; }
    void execute()
    {
        kiss_fft(plan, reinterpret_cast<const kiss_fft_cpx*>(in), reinterpret_cast<kiss_fft_cpx*>(out));
    }
    ~fft_benchmark() { free(plan); }
private:
    kiss_fft_cfg plan;
    real* out;
    const real* in;
    unsigned char* temp;
};
