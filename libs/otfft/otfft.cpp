/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "fft_benchmark.hpp"
#include <cstring>
#include <otfft.h>
#include <string>

std::string fft_name() { return "otfft"; }

// OTFFT only supports double precision.
// complex_t = { double Re, Im } — same layout as interleaved double pairs.

template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
};

// ---- complex double, forward, 1D -------------------------------------------
template <bool inplace>
class fft_implementation<1, double, true, false, inplace> : public fft_impl<double>
{
public:
    explicit fft_implementation(sizes_t<1> sizes) : N(sizes[0])
    {
        plan = OTFFT::Factory::createComplexFFT(static_cast<int>(N));
    }

    void execute(double* out, const double* in) override
    {
        if (out != in)
            std::memcpy(out, in, N * sizeof(OTFFT::complex_t));
        plan->fwd0(reinterpret_cast<OTFFT::complex_t*>(out));
    }

private:
    size_t N;
    OTFFT::ComplexFFTPtr plan;
};

// ---- complex double, inverse, 1D -------------------------------------------
template <bool inplace>
class fft_implementation<1, double, true, true, inplace> : public fft_impl<double>
{
public:
    explicit fft_implementation(sizes_t<1> sizes) : N(sizes[0])
    {
        plan = OTFFT::Factory::createComplexFFT(static_cast<int>(N));
    }

    void execute(double* out, const double* in) override
    {
        if (out != in)
            std::memcpy(out, in, N * sizeof(OTFFT::complex_t));
        plan->inv0(reinterpret_cast<OTFFT::complex_t*>(out));
    }

private:
    size_t N;
    OTFFT::ComplexFFTPtr plan;
};

// ---- real double, forward (real→complex), 1D --------------------------------
// OTFFT::RealFFT::fwd0(x, y): x is real input (N doubles), y is complex output.
// Benchmark layout: output is (N+2) doubles = (N/2+1) interleaved complex pairs.
template <bool inplace>
class fft_implementation<1, double, false, false, inplace> : public fft_impl<double>
{
public:
    explicit fft_implementation(sizes_t<1> sizes) : N(sizes[0])
    {
        plan = OTFFT::Factory::createRealFFT(static_cast<int>(N));
    }

    void execute(double* out, const double* in) override
    {
        plan->fwd0(const_cast<double*>(in), reinterpret_cast<OTFFT::complex_t*>(out));
        reinterpret_cast<OTFFT::complex_t*>(out)[N / 2].Im = 0.0; // Nyquist imaginary is always zero
    }

private:
    size_t N;
    OTFFT::RealFFTPtr plan;
};

// ---- real double, inverse (complex→real), 1D --------------------------------
// OTFFT::RealFFT::inv0(y, x): y is complex input (N/2+1 bins), x is real output.
// inv0 destroys its input buffer.
template <bool inplace>
class fft_implementation<1, double, false, true, inplace> : public fft_impl<double>
{
public:
    explicit fft_implementation(sizes_t<1> sizes) : N(sizes[0])
    {
        plan = OTFFT::Factory::createRealFFT(static_cast<int>(N));
    }

    void execute(double* out, const double* in) override
    {
        // inv0 destroys its input; copy to out first if not in-place.
        if (out != in)
            std::memcpy(out, in, (N / 2 + 1) * sizeof(OTFFT::complex_t));
        plan->inv0(reinterpret_cast<OTFFT::complex_t*>(out), out);
    }

private:
    size_t N;
    OTFFT::RealFFTPtr plan;
};

// ---- fft_create entry point -------------------------------------------------

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    // OTFFT only supports double precision
    if constexpr (sizeof(real) != sizeof(double))
        return nullptr;

    if (size.size() != 1)
        return nullptr; // OTFFT only supports 1D transforms

    const size_t N = size[0];
    if (N == 0)
        return nullptr;

    // Real FFT requires even length
    if (!is_complex && (N % 2) != 0)
        return nullptr;

    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
