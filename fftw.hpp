/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016 D Levin
 * Benchmark source code is dual-licensed under MIT and GPL 2 or later
 * See LICENSE.txt for details
 */
#pragma once

#include "fftw/fftw3.h"
#include "benchmark.hpp"
#include <string>

#ifdef TYPE_FLOAT

template <bool invert>
class fft_benchmark
{
public:
    fft_benchmark(size_t size, real* out, const real* in)
        : out(out), in(in)
    {
        plan = fftwf_plan_dft_1d( size,
                               ( fftwf_complex* )in,
                               ( fftwf_complex* )out,
                               invert ? FFTW_BACKWARD : FFTW_FORWARD,
                               FFTW_ESTIMATE );
    }
    static std::string name() { return fftwf_version; }
    static std::string shortname() { return "FFTW"; }
    void execute()
    {
        fftwf_execute( plan );
    }
    ~fft_benchmark() { fftwf_destroy_plan(plan); }
private:
    fftwf_plan plan;
    real* out;
    const real* in;
    unsigned char* temp;
};

#else

template <bool invert>
class fft_benchmark
{
public:
    fft_benchmark(size_t size, real* out, const real* in)
        : out(out), in(in)
    {
        plan = fftw_plan_dft_1d( size,
                               ( fftw_complex* )in,
                               ( fftw_complex* )out,
                               invert ? FFTW_BACKWARD : FFTW_FORWARD,
                               FFTW_ESTIMATE );
    }
    static std::string name() { return fftw_version; }
    static std::string shortname() { return "FFTW"; }
    void execute()
    {
        fftw_execute( plan );
    }
    ~fft_benchmark() { fftw_destroy_plan(plan); }
private:
    fftw_plan plan;
    real* out;
    const real* in;
    unsigned char* temp;
};
#endif
