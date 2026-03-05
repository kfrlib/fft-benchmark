/**
 * FFT bencmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "src_benchmark.hpp"
#include <soxr.h>
#include <string>

std::string src_name()
{
    return std::string("soxr ") + soxr_version();
}

template <typename real>
struct src_implementation;

template <>
struct src_implementation<float> : public src_impl<float>
{
    src_implementation(unsigned out_rate, unsigned in_rate, unsigned seconds)
    {
        in_length  = static_cast<size_t>(in_rate) * seconds;
        out_length = static_cast<size_t>(out_rate) * seconds;

        soxr_error_t error = nullptr;

        // VHQ (28-bit) with linear phase and steep filter for shortest transition
        soxr_quality_spec_t q = soxr_quality_spec(SOXR_VHQ | SOXR_LINEAR_PHASE | SOXR_STEEP_FILTER, 0);
        soxr_io_spec_t io     = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);

        resampler = soxr_create(static_cast<double>(in_rate), static_cast<double>(out_rate), 1, &error,
                                &io, &q, nullptr);
        if (error)
        {
            fprintf(stderr, "soxr: soxr_create failed: %s\n", soxr_strerror(error));
            resampler = nullptr;
        }
    }

    void execute(float* out, const float* in) final
    {
        if (!resampler)
            return;

        soxr_clear(resampler);

        size_t idone = 0, odone = 0;
        soxr_error_t error =
            soxr_process(resampler, in, in_length, &idone, out, out_length, &odone);
        if (error)
        {
            fprintf(stderr, "soxr: soxr_process failed: %s\n", soxr_strerror(error));
            return;
        }
        // Flush: signal end-of-input
        soxr_process(resampler, nullptr, 0, nullptr, out + odone, out_length - odone, &odone);
    }

    ~src_implementation()
    {
        if (resampler)
            soxr_delete(resampler);
    }

    soxr_t resampler = nullptr;
    size_t in_length;
    size_t out_length;
};

template <>
struct src_implementation<double> : public src_impl<double>
{
    src_implementation(unsigned out_rate, unsigned in_rate, unsigned seconds)
    {
        in_length  = static_cast<size_t>(in_rate) * seconds;
        out_length = static_cast<size_t>(out_rate) * seconds;

        soxr_error_t error = nullptr;

        // VHQ (28-bit) with linear phase and steep filter for shortest transition
        soxr_quality_spec_t q =
            soxr_quality_spec(SOXR_VHQ | SOXR_LINEAR_PHASE | SOXR_STEEP_FILTER, SOXR_DOUBLE_PRECISION);
        soxr_io_spec_t io = soxr_io_spec(SOXR_FLOAT64_I, SOXR_FLOAT64_I);

        resampler = soxr_create(static_cast<double>(in_rate), static_cast<double>(out_rate), 1, &error,
                                &io, &q, nullptr);
        if (error)
        {
            fprintf(stderr, "soxr: soxr_create failed: %s\n", soxr_strerror(error));
            resampler = nullptr;
        }
    }

    void execute(double* out, const double* in) final
    {
        if (!resampler)
            return;

        soxr_clear(resampler);

        size_t idone = 0, odone = 0;
        soxr_error_t error =
            soxr_process(resampler, in, in_length, &idone, out, out_length, &odone);
        if (error)
        {
            fprintf(stderr, "soxr: soxr_process failed: %s\n", soxr_strerror(error));
            return;
        }
        // Flush: signal end-of-input
        soxr_process(resampler, nullptr, 0, nullptr, out + odone, out_length - odone, &odone);
    }

    ~src_implementation()
    {
        if (resampler)
            soxr_delete(resampler);
    }

    soxr_t resampler = nullptr;
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
