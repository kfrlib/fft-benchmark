/**
 * FFT bencmarking tool (https://www.kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "src_benchmark.hpp"
#include <speex/speex_resampler.h>
#include <string>

std::string src_name()
{
    return std::string("speexdsp (resampler quality 10)");
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

        int err    = 0;
        state      = speex_resampler_init(1 /* channels */, in_rate, out_rate,
                                          SPEEX_RESAMPLER_QUALITY_MAX, &err);
        if (err != RESAMPLER_ERR_SUCCESS || !state)
        {
            fprintf(stderr, "speexdsp: speex_resampler_init failed: %s\n",
                    speex_resampler_strerror(err));
            state = nullptr;
        }
    }

    void execute(float* out, const float* in) final
    {
        if (!state)
            return;

        speex_resampler_reset_mem(state);

        spx_uint32_t in_len  = static_cast<spx_uint32_t>(in_length);
        spx_uint32_t out_len = static_cast<spx_uint32_t>(out_length);

        int err = speex_resampler_process_float(state, 0, in, &in_len, out, &out_len);
        if (err != RESAMPLER_ERR_SUCCESS)
        {
            fprintf(stderr, "speexdsp: speex_resampler_process_float failed: %s\n",
                    speex_resampler_strerror(err));
        }
    }

    ~src_implementation()
    {
        if (state)
            speex_resampler_destroy(state);
    }

    SpeexResamplerState* state = nullptr;
    size_t in_length;
    size_t out_length;
};

template <typename real>
src_impl_ptr<real> src_create(unsigned out_rate, unsigned in_rate, unsigned seconds)
{
    if constexpr (std::is_same_v<real, float>)
        return src_impl_ptr<float>(new src_implementation<float>(out_rate, in_rate, seconds));
    else
        return nullptr; // speexdsp resampler only supports float
}

template std::unique_ptr<src_impl<float>> src_create<float>(unsigned, unsigned, unsigned);
template std::unique_ptr<src_impl<double>> src_create<double>(unsigned, unsigned, unsigned);
