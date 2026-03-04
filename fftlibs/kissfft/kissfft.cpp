/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "kissfft.hh"
#include "fft_benchmark.hpp"
#include <complex>
#include <cstring>
#include <memory>
#include <string>
#include <variant>

std::string fft_name() { return std::string("KISS FFT ") + KISSFFT_VERSION; }

// Default: unsupported configurations
template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
public:
};

// complex, 1D only
template <typename real, bool invert, bool inplace>
class fft_implementation<1, real, true, invert, inplace> : public fft_impl<real>
{
public:
    using cpx_t = std::complex<real>;

    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), fft(sizes[0], invert)
    {
        if constexpr (inplace)
        {
            scratch.resize(N);
        }
    }

    void execute(real* out, const real* in)
    {
        if constexpr (inplace)
        {
            std::memcpy(scratch.data(), in, N * sizeof(cpx_t));
            fft.transform(reinterpret_cast<const cpx_t*>(scratch.data()), reinterpret_cast<cpx_t*>(out));
        }
        else
        {
            fft.transform(reinterpret_cast<const cpx_t*>(in), reinterpret_cast<cpx_t*>(out));
        }
    }

private:
    size_t N;
    kissfft<real> fft;
    std::conditional_t<inplace, std::vector<cpx_t>, std::monostate> scratch;
};

// real forward (real-to-complex), 1D only
// KissFFT transform_real requires nfft = N/2 (it operates on pairs)
template <typename real, bool inplace>
class fft_implementation<1, real, false, false, inplace> : public fft_impl<real>
{
public:
    using cpx_t = std::complex<real>;

    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), fft(sizes[0] / 2, false)
    {
        if constexpr (inplace)
        {
            scratch.resize(N);
        }
    }

    void execute(real* out, const real* in)
    {
        if constexpr (inplace)
        {
            std::memcpy(scratch.data(), in, N * sizeof(real));
            fft.transform_real(scratch.data(), reinterpret_cast<cpx_t*>(out));
        }
        else
        {
            fft.transform_real(in, reinterpret_cast<cpx_t*>(out));
        }
        out[N]     = out[1];
        out[N + 1] = 0;
        out[1]     = 0;
    }

private:
    size_t N;
    kissfft<real> fft;
    std::conditional_t<inplace, std::vector<real>, std::monostate> scratch;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    if (size.size() != 1)
        return nullptr; // only 1D transforms are supported
    if (!is_complex && invert)
        return nullptr; // KissFFT does not support real inverse transforms
    if (!is_complex && size[0] % 2 != 0)
        return nullptr; // KissFFT real transform requires even sizes
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
