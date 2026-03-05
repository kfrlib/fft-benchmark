/**
 * FFT bencmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "src_benchmark.hpp"
#include <samplerate.h>
#include <string>

std::string src_name()
{
    return std::string("libsamplerate ") + src_get_version();
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
        ratio      = static_cast<double>(out_rate) / static_cast<double>(in_rate);

        int error = 0;
        state     = src_new(SRC_SINC_BEST_QUALITY, 1 /* channels */, &error);
        if (error || !state)
        {
            fprintf(stderr, "libsamplerate: src_new failed: %s\n", src_strerror(error));
            state = nullptr;
        }
    }

    void execute(float* out, const float* in) final
    {
        if (!state)
            return;

        src_reset(state);

        SRC_DATA data{};
        data.data_in       = in;
        data.data_out      = out;
        data.input_frames  = static_cast<long>(in_length);
        data.output_frames = static_cast<long>(out_length);
        data.src_ratio     = ratio;
        data.end_of_input  = 1;

        int error = src_process(state, &data);
        if (error)
        {
            fprintf(stderr, "libsamplerate: src_process failed: %s\n", src_strerror(error));
        }
    }

    ~src_implementation()
    {
        if (state)
            src_delete(state);
    }

    SRC_STATE* state = nullptr;
    size_t in_length;
    size_t out_length;
    double ratio;
};

template <typename real>
src_impl_ptr<real> src_create(unsigned out_rate, unsigned in_rate, unsigned seconds)
{
    if constexpr (std::is_same_v<real, float>)
        return src_impl_ptr<float>(new src_implementation<float>(out_rate, in_rate, seconds));
    else
        return nullptr; // libsamplerate only supports float
}

template std::unique_ptr<src_impl<float>> src_create<float>(unsigned, unsigned, unsigned);
template std::unique_ptr<src_impl<double>> src_create<double>(unsigned, unsigned, unsigned);
