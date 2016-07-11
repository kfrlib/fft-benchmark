/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016 D Levin
 * Benchmark source code is dual-licensed under MIT and GPL 2 or later
 * See LICENSE.txt for details
 */
#pragma once

#include "benchmark.hpp"
#include "kfr/dft/fft.hpp"
#include "kfr/version.hpp"
#include <string>

template <bool invert>
class fft_benchmark
{
public:
    fft_benchmark(size_t size, real* out, const real* in)
        : plan(size), out(out), in(in), temp(kfr::aligned_allocate<unsigned char>(plan.temp_size))
    {
    }
    static std::string name() { return kfr::library_version(); }
    static std::string shortname() { return "KFR"; }
    void execute()
    {
         plan.execute(kfr::complex_cast(out), kfr::complex_cast(in), temp, kfr::cbool<invert>);
    }
    ~fft_benchmark() { kfr::aligned_deallocate(temp); }
private:
    kfr::dft_plan<real> plan;
    real* out;
    const real* in;
    unsigned char* temp;
};
