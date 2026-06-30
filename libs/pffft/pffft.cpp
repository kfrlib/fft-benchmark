/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "fft_benchmark.hpp"
#include <memory>
#include <pffft/pffft.h>
#include <string>

std::string fft_name() { return "pffft (pffft_simd_size()=" + std::to_string(pffft_simd_size()) + ")"; }

template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
public:
};

// complex, float only, 1D only
template <bool invert, bool inplace>
class fft_implementation<1, float, true, invert, inplace> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes, float*, const float*) : N(sizes[0])
    {
        setup = pffft_new_setup(static_cast<int>(N), PFFFT_COMPLEX);
        work  = static_cast<float*>(pffft_aligned_malloc(2 * N * sizeof(float)));
    }

    void execute(float* out, const float* in)
    {
        pffft_transform_ordered(setup, in, out, work, invert ? PFFFT_BACKWARD : PFFFT_FORWARD);
    }

    ~fft_implementation()
    {
        pffft_aligned_free(work);
        pffft_destroy_setup(setup);
    }

private:
    size_t N;
    PFFFT_Setup* setup;
    float* work;
};

// real forward (real-to-complex), float only, 1D only
// pffft native real layout (hc): N floats
//   [Re0, ReNyq, Re1, Im1, Re2, Im2, ..., Re(N/2-1), Im(N/2-1)]  (even N)
// No conversion is performed — the benchmark adapts to the reported layout.
template <bool inplace>
class fft_implementation<1, float, false, false, inplace> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes, float*, const float*) : N(sizes[0])
    {
        setup = pffft_new_setup(static_cast<int>(N), PFFFT_REAL);
        work  = static_cast<float*>(pffft_aligned_malloc(N * sizeof(float)));
    }

    real_layout layout() const final { return real_layout::hc; }

    void execute(float* out, const float* in)
    {
        pffft_transform_ordered(setup, in, out, work, PFFFT_FORWARD);
    }

    ~fft_implementation()
    {
        pffft_aligned_free(work);
        pffft_destroy_setup(setup);
    }

private:
    size_t N;
    PFFFT_Setup* setup;
    float* work;
};

// real backward (complex-to-real), float only, 1D only
// Consumes the hc layout above; no conversion.
template <bool inplace>
class fft_implementation<1, float, false, true, inplace> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes, float*, const float*) : N(sizes[0])
    {
        setup = pffft_new_setup(static_cast<int>(N), PFFFT_REAL);
        work  = static_cast<float*>(pffft_aligned_malloc(N * sizeof(float)));
    }

    real_layout layout() const final { return real_layout::hc; }

    void execute(float* out, const float* in)
    {
        pffft_transform_ordered(setup, in, out, work, PFFFT_BACKWARD);
    }

    ~fft_implementation()
    {
        pffft_aligned_free(work);
        pffft_destroy_setup(setup);
    }

private:
    size_t N;
    PFFFT_Setup* setup;
    float* work;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, real* out, const real* in, bool is_complex,
                              bool invert, bool inplace)
{
    /* unfortunately, the fft size must be a multiple of 16 for complex FFTs
       and 32 for real FFTs -- a lot of stuff would need to be rewritten to
       handle other cases (or maybe just switch to a scalar fft, I don't know..) */
    if (size.size() != 1)
        return nullptr; // pffft only supports 1D transforms
    if (is_complex && (size[0] % (pffft_simd_size() * pffft_simd_size())) != 0)
        return nullptr; // pffft complex transform requires sizes that are multiples of SIMD_SZ^2
    if (!is_complex && (size[0] % (2 * pffft_simd_size() * pffft_simd_size())) != 0)
        return nullptr; // pffft real transform requires sizes that are multiples of 2*SIMD_SZ^2
    return fft_create_for<fft_implementation, real>(size, out, in, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, float*,
                                                            const float*, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, double*,
                                                              const double*, bool, bool, bool);
