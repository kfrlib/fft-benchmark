/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2023 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "benchmark.hpp"
#include <array>
#include <fftw3.h>
#include <memory>
#include <string>

std::string fft_name() { return std::string(fftw_version); }

template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
public:
};

// complex
template <int dims, typename real, bool invert, bool inplace>
class fft_implementation<dims, real, true, invert, inplace> : public fft_impl<real>
{
public:
    PICK;

    using Complex                         = pick_t<fftwf_complex, fftw_complex>;
    using Plan                            = pick_t<fftwf_plan, fftw_plan>;
    constexpr static auto fn_plan_dft_1d  = pick(fftwf_plan_dft_1d, fftw_plan_dft_1d);
    constexpr static auto fn_plan_dft_2d  = pick(fftwf_plan_dft_2d, fftw_plan_dft_2d);
    constexpr static auto fn_plan_dft_3d  = pick(fftwf_plan_dft_3d, fftw_plan_dft_3d);
    constexpr static auto fn_execute_dft  = pick(fftwf_execute_dft, fftw_execute_dft);
    constexpr static auto fn_destroy_plan = pick(fftwf_destroy_plan, fftw_destroy_plan);
    constexpr static auto fn_print_plan   = pick(fftwf_print_plan, fftw_print_plan);

    fft_implementation(sizes_t<dims> sizes)
    {
        if constexpr (dims == 1)
        {
            plan = fn_plan_dft_1d(sizes[0], nullptr, nullptr, invert ? FFTW_BACKWARD : FFTW_FORWARD,
                                  FFTW_ESTIMATE);
        }
        else if constexpr (dims == 2)
        {
            plan = fn_plan_dft_2d(sizes[0], sizes[1], nullptr, nullptr, invert ? FFTW_BACKWARD : FFTW_FORWARD,
                                  FFTW_ESTIMATE);
        }
        else if constexpr (dims == 3)
        {
            plan = fn_plan_dft_3d(sizes[0], sizes[1], sizes[2], nullptr, nullptr,
                                  invert ? FFTW_BACKWARD : FFTW_FORWARD, FFTW_ESTIMATE);
        }
    }

    void execute(real* out, const real* in) { fn_execute_dft(plan, (Complex*)in, (Complex*)out); }
    ~fft_implementation() { fn_destroy_plan(plan); }

private:
    Plan plan;
};

// real-to-complex
template <int dims, typename real, bool inplace>
class fft_implementation<dims, real, false, false, inplace> : public fft_impl<real>
{
public:
    PICK;

    using Complex                            = pick_t<fftwf_complex, fftw_complex>;
    using Plan                               = pick_t<fftwf_plan, fftw_plan>;
    constexpr static auto fn_plan_dft_r2c_1d = pick(fftwf_plan_dft_r2c_1d, fftw_plan_dft_r2c_1d);
    constexpr static auto fn_plan_dft_r2c_2d = pick(fftwf_plan_dft_r2c_2d, fftw_plan_dft_r2c_2d);
    constexpr static auto fn_plan_dft_r2c_3d = pick(fftwf_plan_dft_r2c_3d, fftw_plan_dft_r2c_3d);
    constexpr static auto fn_execute_dft_r2c = pick(fftwf_execute_dft_r2c, fftw_execute_dft_r2c);
    constexpr static auto fn_destroy_plan    = pick(fftwf_destroy_plan, fftw_destroy_plan);
    constexpr static auto fn_print_plan      = pick(fftwf_print_plan, fftw_print_plan);

    fft_implementation(sizes_t<dims> sizes)
    {
        if constexpr (dims == 1)
        {
            plan = fn_plan_dft_r2c_1d(sizes[0], nullptr, nullptr, FFTW_ESTIMATE);
        }
        else if constexpr (dims == 2)
        {
            plan = fn_plan_dft_r2c_2d(sizes[0], sizes[1], nullptr, nullptr, FFTW_ESTIMATE);
        }
        else if constexpr (dims == 3)
        {
            plan = fn_plan_dft_r2c_3d(sizes[0], sizes[1], sizes[2], nullptr, nullptr, FFTW_ESTIMATE);
        }
    }

    void execute(real* out, const real* in) { fn_execute_dft_r2c(plan, (real*)in, (Complex*)out); }
    ~fft_implementation() { fn_destroy_plan(plan); }

private:
    Plan plan;
};

// complex-to-real
template <int dims, typename real, bool inplace>
class fft_implementation<dims, real, false, true, inplace> : public fft_impl<real>
{
public:
    PICK;

    using Complex                            = pick_t<fftwf_complex, fftw_complex>;
    using Plan                               = pick_t<fftwf_plan, fftw_plan>;
    constexpr static auto fn_plan_dft_c2r_1d = pick(fftwf_plan_dft_c2r_1d, fftw_plan_dft_c2r_1d);
    constexpr static auto fn_plan_dft_c2r_2d = pick(fftwf_plan_dft_c2r_2d, fftw_plan_dft_c2r_2d);
    constexpr static auto fn_plan_dft_c2r_3d = pick(fftwf_plan_dft_c2r_3d, fftw_plan_dft_c2r_3d);
    constexpr static auto fn_execute_dft_c2r = pick(fftwf_execute_dft_c2r, fftw_execute_dft_c2r);
    constexpr static auto fn_destroy_plan    = pick(fftwf_destroy_plan, fftw_destroy_plan);
    constexpr static auto fn_print_plan      = pick(fftwf_print_plan, fftw_print_plan);

    fft_implementation(sizes_t<dims> sizes)
    {
        if constexpr (dims == 1)
        {
            plan = fn_plan_dft_c2r_1d(sizes[0], nullptr, nullptr, FFTW_ESTIMATE);
        }
        else if constexpr (dims == 2)
        {
            plan = fn_plan_dft_c2r_2d(sizes[0], sizes[1], nullptr, nullptr, FFTW_ESTIMATE);
        }
        else if constexpr (dims == 3)
        {
            plan = fn_plan_dft_c2r_3d(sizes[0], sizes[1], sizes[2], nullptr, nullptr, FFTW_ESTIMATE);
        }
    }

    void execute(real* out, const real* in) { fn_execute_dft_c2r(plan, (Complex*)in, (real*)out); }
    ~fft_implementation() { fn_destroy_plan(plan); }

private:
    Plan plan;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
