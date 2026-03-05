/**
 * FFT benchmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "CDSPResampler.h"
#include "src_benchmark.hpp"
#include <cstring>
#include <string>


std::string src_name() { return std::string("r8brain-free-src ") + std::string(R8B_VERSION); }

template <typename real>
struct src_implementation;

template <>
struct src_implementation<double> : public src_impl<double>
{
    src_implementation(unsigned out_rate, unsigned in_rate, unsigned seconds)
    {
        in_length  = static_cast<size_t>(in_rate) * seconds;
        out_length = static_cast<size_t>(out_rate) * seconds;

        resampler = new r8b::CDSPResampler(static_cast<double>(in_rate), static_cast<double>(out_rate),
                                           static_cast<int>(in_length));
    }

    void execute(double* out, const double* in) final
    {
        resampler->clear();

        // r8brain process() takes non-const input pointer
        in_buf.assign(in, in + in_length);

        double* inp          = in_buf.data();
        double* outp         = out;
        size_t in_remaining  = in_length;
        size_t out_remaining = out_length;

        while (in_remaining > 0 && out_remaining > 0)
        {
            int block = static_cast<int>(std::min<size_t>(in_remaining, 4096));
            double* optr;
            int out_count = resampler->process(inp, block, optr);

            if (out_count > 0)
            {
                size_t to_copy = std::min<size_t>(static_cast<size_t>(out_count), out_remaining);
                std::memcpy(outp, optr, to_copy * sizeof(double));
                outp += to_copy;
                out_remaining -= to_copy;
            }

            inp += block;
            in_remaining -= block;
        }
    }

    ~src_implementation() { delete resampler; }

    r8b::CDSPResampler* resampler = nullptr;
    std::vector<double> in_buf;
    size_t in_length;
    size_t out_length;
};

template <>
struct src_implementation<float> : public src_impl<float>
{
    src_implementation(unsigned out_rate, unsigned in_rate, unsigned seconds)
    {
        in_length  = static_cast<size_t>(in_rate) * seconds;
        out_length = static_cast<size_t>(out_rate) * seconds;

        resampler = new r8b::CDSPResampler24(static_cast<double>(in_rate), static_cast<double>(out_rate),
                                             static_cast<int>(in_length));

        in_buf.resize(in_length);
        out_buf.resize(out_length);
    }

    void execute(float* out, const float* in) final
    {
        // Convert float input to double
        for (size_t i = 0; i < in_length; ++i)
            in_buf[i] = static_cast<double>(in[i]);

        resampler->clear();

        double* inp          = in_buf.data();
        double* outp         = out_buf.data();
        size_t in_remaining  = in_length;
        size_t out_remaining = out_length;

        while (in_remaining > 0 && out_remaining > 0)
        {
            int block = static_cast<int>(std::min<size_t>(in_remaining, 4096));
            double* optr;
            int out_count = resampler->process(inp, block, optr);

            if (out_count > 0)
            {
                size_t to_copy = std::min<size_t>(static_cast<size_t>(out_count), out_remaining);
                std::memcpy(outp, optr, to_copy * sizeof(double));
                outp += to_copy;
                out_remaining -= to_copy;
            }

            inp += block;
            in_remaining -= block;
        }

        // Convert double output to float
        size_t written = static_cast<size_t>(outp - out_buf.data());
        for (size_t i = 0; i < written; ++i)
            out[i] = static_cast<float>(out_buf[i]);
    }

    ~src_implementation() { delete resampler; }

    r8b::CDSPResampler24* resampler = nullptr;
    std::vector<double> in_buf;
    std::vector<double> out_buf;
    size_t in_length;
    size_t out_length;
};

template <typename real>
src_impl_ptr<real> src_create(unsigned out_rate, unsigned in_rate, unsigned seconds)
{
    return src_impl_ptr<real>(new src_implementation<real>(out_rate, in_rate, seconds));
}

template std::unique_ptr<src_impl<float>> src_create<float>(unsigned, unsigned, unsigned);
template std::unique_ptr<src_impl<double>> src_create<double>(unsigned, unsigned, unsigned);
