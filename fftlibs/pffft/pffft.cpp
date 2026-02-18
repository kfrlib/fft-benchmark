/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "benchmark.hpp"
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
    fft_implementation(sizes_t<1> sizes) : N(sizes[0])
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
template <bool inplace>
class fft_implementation<1, float, false, false, inplace> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes) : N(sizes[0])
    {
        setup = pffft_new_setup(static_cast<int>(N), PFFFT_REAL);
        work  = static_cast<float*>(pffft_aligned_malloc(N * sizeof(float)));
    }

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
template <bool inplace>
class fft_implementation<1, float, false, true, inplace> : public fft_impl<float>
{
public:
    fft_implementation(sizes_t<1> sizes) : N(sizes[0])
    {
        setup = pffft_new_setup(static_cast<int>(N), PFFFT_REAL);
        work  = static_cast<float*>(pffft_aligned_malloc(N * sizeof(float)));
    }

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
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
