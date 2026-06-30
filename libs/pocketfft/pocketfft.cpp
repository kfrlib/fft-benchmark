/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#define POCKETFFT_NO_MULTITHREADING 1

#include "fft_benchmark.hpp"
#include <cstring>
#include <pocketfft_hdronly.h>
#include <string>

std::string fft_name()
{
    return "pocketfft (" + std::to_string(pocketfft::detail::VLEN<float>::val * 32) + " bits VLEN)";
}

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
    using cmplx_t = pocketfft::detail::cmplx<real>;

    fft_implementation(sizes_t<1> sizes, real*, const real*) : N(sizes[0]), plan(sizes[0]) {}

    void execute(real* out, const real* in)
    {
        if constexpr (!inplace)
            std::memcpy(out, in, N * 2 * sizeof(real));
        plan.exec(reinterpret_cast<cmplx_t*>(out), real(1), !invert);
    }

private:
    size_t N;
    pocketfft::detail::pocketfft_c<real> plan;
};

// real forward (real-to-complex), 1D only
// pocketfft_r::exec produces the native FFTPACK packed layout (N reals):
//   even N: [DC, r1, i1, r2, i2, ..., Nyq]
//   odd  N: [DC, r1, i1, ..., r((N-1)/2), i((N-1)/2)]
// No conversion is performed — the benchmark adapts to the reported layout.
template <typename real, bool inplace>
class fft_implementation<1, real, false, false, inplace> : public fft_impl<real>
{
public:
    fft_implementation(sizes_t<1> sizes, real*, const real*) : N(sizes[0]), plan(sizes[0]) {}

    real_layout layout() const final { return real_layout::fftpack; }

    void execute(real* out, const real* in)
    {
        if constexpr (!inplace)
            std::memcpy(out, in, N * sizeof(real));
        plan.exec(out, real(1), true);
    }

private:
    size_t N;
    pocketfft::detail::pocketfft_r<real> plan;
};

// real backward (complex-to-real), 1D only
// Consumes the FFTPACK packed layout above; no conversion.
template <typename real, bool inplace>
class fft_implementation<1, real, false, true, inplace> : public fft_impl<real>
{
public:
    fft_implementation(sizes_t<1> sizes, real*, const real*) : N(sizes[0]), plan(sizes[0]) {}

    real_layout layout() const final { return real_layout::fftpack; }

    void execute(real* out, const real* in)
    {
        if constexpr (!inplace)
            std::memcpy(out, in, N * sizeof(real));
        plan.exec(out, real(1), false);
    }

private:
    size_t N;
    pocketfft::detail::pocketfft_r<real> plan;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, real* out, const real* in, bool is_complex,
                              bool invert, bool inplace)
{
    if (size.size() != 1)
        return nullptr; // pocketfft wrapper only supports 1D transforms
    return fft_create_for<fft_implementation, real>(size, out, in, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, float*,
                                                            const float*, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, double*,
                                                              const double*, bool, bool, bool);
