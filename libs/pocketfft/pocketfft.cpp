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

std::string fft_name() { return "pocketfft"; }

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

    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), plan(sizes[0]) {}

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
// pocketfft_r::exec produces FFTPACK packed format: [DC, r1, i1, r2, i2, ..., Nyq(even N)]
// The benchmark expects standard interleaved: [DC, 0, r1, i1, ..., Nyq, 0]
template <typename real, bool inplace>
class fft_implementation<1, real, false, false, inplace> : public fft_impl<real>
{
public:
    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), plan(sizes[0]) {}

    void execute(real* out, const real* in)
    {
        if constexpr (!inplace)
            std::memcpy(out, in, N * sizeof(real));
        plan.exec(out, real(1), true);
        // Convert FFTPACK → standard complex in-place, working backward to avoid overwrites
        if (N % 2 == 0)
        {
            out[N]     = out[N - 1]; // Nyquist real
            out[N + 1] = real(0);
        }
        for (ptrdiff_t k = (ptrdiff_t)((N - 1) / 2); k >= 1; --k)
        {
            real re        = out[2 * k - 1];
            real im        = out[2 * k];
            out[2 * k + 1] = im;
            out[2 * k]     = re;
        }
        out[1] = real(0); // DC imaginary = 0
    }

private:
    size_t N;
    pocketfft::detail::pocketfft_r<real> plan;
};

// real backward (complex-to-real), 1D only
template <typename real, bool inplace>
class fft_implementation<1, real, false, true, inplace> : public fft_impl<real>
{
public:
    fft_implementation(sizes_t<1> sizes) : N(sizes[0]), plan(sizes[0]) {}

    void execute(real* out, const real* in)
    {
        // Convert standard complex → FFTPACK packed format
        // Forward pass: write position 2k-1 < read position 2k, so safe for in==out
        if constexpr (!inplace)
            out[0] = in[0];
        for (size_t k = 1; k <= (N - 1) / 2; ++k)
        {
            out[2 * k - 1] = in[2 * k];
            out[2 * k]     = in[2 * k + 1];
        }
        if (N % 2 == 0)
            out[N - 1] = in[N]; // Nyquist real
        plan.exec(out, real(1), false);
    }

private:
    size_t N;
    pocketfft::detail::pocketfft_r<real> plan;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    if (size.size() != 1)
        return nullptr; // pocketfft wrapper only supports 1D transforms
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
