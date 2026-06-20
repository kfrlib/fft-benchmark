/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "fft_benchmark.hpp"
#include "src_benchmark.hpp"
#include <cstring>
#include <juce_dsp/juce_dsp.h>
#include <string>

std::string fft_name()
{
    return "JUCE " + std::string(JUCE_STRINGIFY(JUCE_MAJOR_VERSION)) + "." +
           std::string(JUCE_STRINGIFY(JUCE_MINOR_VERSION)) + "." +
           std::string(JUCE_STRINGIFY(JUCE_BUILDNUMBER));
}

static int log2_exact(size_t n)
{
    if (n == 0 || (n & (n - 1)) != 0)
        return -1; // not a power of 2
    int order = 0;
    while ((size_t(1) << order) < n)
        ++order;
    return order;
}

// Default: unsupported configurations
template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
public:
};

// complex, float only, 1D only
template <bool invert>
class fft_implementation<1, float, true, invert, false> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), fft(log2_exact(sizes[0])) {}

    void execute(float* out, const float* in)
    {
        fft.perform(reinterpret_cast<const juce::dsp::Complex<float>*>(in),
                    reinterpret_cast<juce::dsp::Complex<float>*>(out), invert);
    }
    fft_scaling scaling() const final { return fft_scaling::inverse_n; }

private:
    size_t N;
    juce::dsp::FFT fft;
};

// real forward (real-to-complex), float only, 1D only
template <>
class fft_implementation<1, float, false, false, true> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), fft(log2_exact(sizes[0])) {}

    void execute(float* out, const float*) { fft.performRealOnlyForwardTransform(out); }
    fft_scaling scaling() const final { return fft_scaling::inverse_n; }

private:
    size_t N;
    juce::dsp::FFT fft;
};

// real backward (complex-to-real), float only, 1D only
template <>
class fft_implementation<1, float, false, true, true> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), fft(log2_exact(sizes[0])) {}

    void execute(float* out, const float*) { fft.performRealOnlyInverseTransform(out); }
    fft_scaling scaling() const final { return fft_scaling::inverse_n; }

private:
    size_t N;
    juce::dsp::FFT fft;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    // JUCE FFT only supports 1D, power-of-2 sizes
    if (size.size() != 1)
        return nullptr; // only 1D transforms are supported
    if (log2_exact(size[0]) < 0)
        return nullptr; // size must be a power of 2
    if (is_complex && inplace)
        return nullptr; // no in-place complex transforms
    if (!is_complex && !inplace)
        return nullptr; // no out-of-place real transforms
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);

std::string src_name()
{
    return "JUCE " + std::string(JUCE_STRINGIFY(JUCE_MAJOR_VERSION)) + "." +
           std::string(JUCE_STRINGIFY(JUCE_MINOR_VERSION)) + "." +
           std::string(JUCE_STRINGIFY(JUCE_BUILDNUMBER)) + " (WindowedSinc)";
}

template <typename real>
struct src_implementation;

template <>
struct src_implementation<float> : public src_impl<float>
{
    src_implementation(unsigned out_rate, unsigned in_rate, unsigned seconds)
    {
        out_length  = out_rate * seconds;
        in_length   = in_rate * seconds;
        speed_ratio = static_cast<double>(in_rate) / static_cast<double>(out_rate);
        interpolator.reset();
    }

    void execute(float* out, const float* in) final
    {
        interpolator.reset();
        interpolator.process(speed_ratio, in, out, static_cast<int>(out_length));
    }

    juce::WindowedSincInterpolator interpolator;
    double speed_ratio;
    size_t out_length;
    size_t in_length;
};

template <typename real>
src_impl_ptr<real> src_create(unsigned out_rate, unsigned in_rate, unsigned seconds)
{
    if constexpr (std::is_same_v<real, float>)
        return src_impl_ptr<float>(new src_implementation<float>(out_rate, in_rate, seconds));
    else
        return nullptr; // JUCE interpolators only support float
}

template std::unique_ptr<src_impl<float>> src_create<float>(unsigned, unsigned, unsigned);
template std::unique_ptr<src_impl<double>> src_create<double>(unsigned, unsigned, unsigned);