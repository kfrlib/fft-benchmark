/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "benchmark.hpp"
#include <memory>

#define SLEEF_STATIC_LIBS
#include <sleefdft.h>
#include <string>

std::string fft_name() { return std::string("Sleef DFT ") + SLEEF_VERSION; }

// Default: unsupported configurations
template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
public:
};

constexpr static uint32_t dft_flags = SLEEF_MODE_ESTIMATE | SLEEF_MODE_NO_MT;

// complex, 1D
template <typename real, bool invert, bool inplace>
class fft_implementation<1, real, true, invert, inplace> : public fft_impl<real>
{
public:
    PICK;

    using InitFn = pick_t<decltype(&SleefDFT_float_init1d), decltype(&SleefDFT_double_init1d)>;
    using ExecFn = pick_t<decltype(&SleefDFT_float_execute), decltype(&SleefDFT_double_execute)>;

    constexpr static InitFn fn_init    = pick(&SleefDFT_float_init1d, &SleefDFT_double_init1d);
    constexpr static ExecFn fn_execute = pick(&SleefDFT_float_execute, &SleefDFT_double_execute);

    fft_implementation(sizes_t<1> sizes)
    {
        uint64_t mode = dft_flags;
        mode |= invert ? SLEEF_MODE_BACKWARD : SLEEF_MODE_FORWARD;
        plan = fn_init(static_cast<uint32_t>(sizes[0]), nullptr, nullptr, mode);
        if (!plan)
        {
            fprintf(stderr, "Sleef DFT: failed to initialize plan for size %zu\n", sizes[0]);
            std::abort();
        }
    }

    void execute(real* out, const real* in) { fn_execute(plan, in, out); }

    ~fft_implementation()
    {
        if (plan)
            SleefDFT_dispose(plan);
    }

private:
    struct SleefDFT* plan = nullptr;
};

// real forward (real-to-complex), 1D
template <typename real, bool inplace>
class fft_implementation<1, real, false, false, inplace> : public fft_impl<real>
{
public:
    PICK;

    using InitFn = pick_t<decltype(&SleefDFT_float_init1d), decltype(&SleefDFT_double_init1d)>;
    using ExecFn = pick_t<decltype(&SleefDFT_float_execute), decltype(&SleefDFT_double_execute)>;

    constexpr static InitFn fn_init    = pick(&SleefDFT_float_init1d, &SleefDFT_double_init1d);
    constexpr static ExecFn fn_execute = pick(&SleefDFT_float_execute, &SleefDFT_double_execute);

    fft_implementation(sizes_t<1> sizes)
    {
        uint64_t mode = SLEEF_MODE_REAL | SLEEF_MODE_FORWARD | dft_flags;
        plan          = fn_init(static_cast<uint32_t>(sizes[0]), nullptr, nullptr, mode);
    }

    void execute(real* out, const real* in) { fn_execute(plan, in, out); }

    ~fft_implementation()
    {
        if (plan)
            SleefDFT_dispose(plan);
    }

private:
    struct SleefDFT* plan = nullptr;
};

// real backward (complex-to-real), 1D
template <typename real, bool inplace>
class fft_implementation<1, real, false, true, inplace> : public fft_impl<real>
{
public:
    PICK;

    using InitFn = pick_t<decltype(&SleefDFT_float_init1d), decltype(&SleefDFT_double_init1d)>;
    using ExecFn = pick_t<decltype(&SleefDFT_float_execute), decltype(&SleefDFT_double_execute)>;

    constexpr static InitFn fn_init    = pick(&SleefDFT_float_init1d, &SleefDFT_double_init1d);
    constexpr static ExecFn fn_execute = pick(&SleefDFT_float_execute, &SleefDFT_double_execute);

    fft_implementation(sizes_t<1> sizes)
    {
        uint64_t mode = SLEEF_MODE_REAL | SLEEF_MODE_BACKWARD | dft_flags;
        plan          = fn_init(static_cast<uint32_t>(sizes[0]), nullptr, nullptr, mode);
    }

    void execute(real* out, const real* in) { fn_execute(plan, in, out); }

    ~fft_implementation()
    {
        if (plan)
            SleefDFT_dispose(plan);
    }

private:
    struct SleefDFT* plan = nullptr;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    if (size.size() != 1)
        return nullptr; // Sleef DFT only supports 1D transforms
    if ((size[0] & (size[0] - 1)) != 0)
        return nullptr; // Sleef DFT requires power-of-2 sizes
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
