/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "fft_benchmark.hpp"
#include "kfr/dft/fft.hpp"
#include "kfr/dsp/sample_rate_conversion.hpp"
#include "kfr/version.hpp"
#include "src_benchmark.hpp"
#include <string>

namespace kfr
{
const char* library_version_dft();
const char* library_version_dsp();
} // namespace kfr

std::string fft_name()
{
#if defined(__x86_64__) || defined(_M_X64)
    if (avx2only)
    {
        fprintf(stderr, "KFR: enabling AVX2\n");
        kfr::override_cpu(kfr::cpu_t::avx2);
    }
#endif
    return std::string(kfr::library_version_dft());
}

template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
public:
};

// complex
template <typename real, bool invert, bool inplace>
class fft_implementation<1, real, true, invert, inplace> : public fft_impl<real>
{
public:
    PICK;
    fft_implementation(sizes_t<1> size)
        : plan(size[0]), temp(kfr::aligned_allocate<unsigned char>(plan.temp_size))
    {
    }
    void execute(real* out, const real* in) final
    {
        plan.execute(kfr::ptr_cast<kfr::complex<real>>(out), kfr::ptr_cast<kfr::complex<real>>(in), temp,
                     kfr::cbool<invert>);
    }
    ~fft_implementation() { kfr::aligned_deallocate(temp); }

private:
    kfr::dft_plan<real> plan;
    unsigned char* temp;
};

// real
template <typename real, bool invert, bool inplace>
class fft_implementation<1, real, false, invert, inplace> : public fft_impl<real>
{
public:
    PICK;
    fft_implementation(sizes_t<1> size)
        : plan(size[0]), temp(kfr::aligned_allocate<unsigned char>(plan.temp_size))
    {
    }
    void execute(real* out, const real* in) final
    {
        if constexpr (invert)
            plan.execute(out, kfr::ptr_cast<kfr::complex<real>>(in), temp, kfr::cbool<invert>);
        else
            plan.execute(kfr::ptr_cast<kfr::complex<real>>(out), in, temp, kfr::cbool<invert>);
    }
    ~fft_implementation() { kfr::aligned_deallocate(temp); }

private:
    kfr::dft_plan_real<real> plan;
    unsigned char* temp;
};

// complex, multidimensional
template <int dims, typename real, bool invert, bool inplace>
class fft_implementation<dims, real, true, invert, inplace> : public fft_impl<real>
{
public:
    PICK;
    fft_implementation(sizes_t<dims> sizes)
        : plan(kfr::shape<dims>::from_std_array(sizes)),
          temp(kfr::aligned_allocate<unsigned char>(plan.temp_size))
    {
    }
    void execute(real* out, const real* in) final
    {
        plan.execute(kfr::ptr_cast<kfr::complex<real>>(out), kfr::ptr_cast<kfr::complex<real>>(in), temp,
                     kfr::cbool<invert>);
    }
    ~fft_implementation() { kfr::aligned_deallocate(temp); }

private:
    kfr::dft_plan_md<real, dims> plan;
    unsigned char* temp;
};

// real, multidimensional
template <int dims, typename real, bool invert, bool inplace>
class fft_implementation<dims, real, false, invert, inplace> : public fft_impl<real>
{
public:
    PICK;
    fft_implementation(sizes_t<dims> sizes)
        : plan(kfr::shape<dims>::from_std_array(sizes), true),
          temp(kfr::aligned_allocate<unsigned char>(plan.temp_size))
    {
    }
    void execute(real* out, const real* in) final
    {
        if constexpr (invert)
            plan.execute(out, kfr::ptr_cast<kfr::complex<real>>(in), temp, kfr::cbool<invert>);
        else
            plan.execute(kfr::ptr_cast<kfr::complex<real>>(out), in, temp, kfr::cbool<invert>);
    }
    ~fft_implementation() { kfr::aligned_deallocate(temp); }

private:
    kfr::dft_plan_md_real<real, dims> plan;
    unsigned char* temp;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    if (!is_complex)
    {
        size_t s = std::accumulate(size.begin(), size.end(), size_t(1), std::multiplies<>{});
        if (s & 1)
            return nullptr; // KFR real transform requires even sizes
    }
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);

std::string src_name()
{
    double att =
        kfr::samplerate_converter<double>::sidelobe_attenuation(kfr::sample_rate_conversion_quality::high);
    double trans =
        kfr::samplerate_converter<double>::transition_width(kfr::sample_rate_conversion_quality::high);
    unsigned order =
        kfr::samplerate_converter<double>::filter_order(kfr::sample_rate_conversion_quality::high);
    double param = kfr::samplerate_converter<double>::window_param(kfr::sample_rate_conversion_quality::high);
    printf("sidelobe attenuation: %f dB, transition width: %f radians, filter order: %u, param = %f\n", att,
           trans, order, param);

#if defined(__x86_64__) || defined(_M_X64)
    if (avx2only)
    {
        fprintf(stderr, "KFR: enabling AVX2\n");
        kfr::override_cpu(kfr::cpu_t::avx2);
    }
#endif
    return std::string(kfr::library_version_dsp());
}

template <typename real>
struct src_implementation : public src_impl<real>
{
    src_implementation(unsigned out_rate, unsigned in_rate, unsigned seconds)
    {
        out_length = out_rate * seconds;
        in_length  = in_rate * seconds;
        converter.reset(new kfr::samplerate_converter<real>(kfr::sample_rate_conversion_quality::high,
                                                            out_rate, in_rate));
    }
    void execute(real* out, const real* in) final
    {
        std::ignore = converter->process(kfr::make_univector(out, out_length).ref(),
                                         kfr::make_univector(in, in_length));
    }

    std::unique_ptr<kfr::samplerate_converter<real>> converter;
    size_t out_length;
    size_t in_length;
};

template <typename real>
src_impl_ptr<real> src_create(unsigned out_rate, unsigned in_rate, unsigned seconds)
{
    return src_impl_ptr<real>(new src_implementation<real>(out_rate, in_rate, seconds));
}

template std::unique_ptr<src_impl<float>> src_create<float>(unsigned, unsigned, unsigned);
template std::unique_ptr<src_impl<double>> src_create<double>(unsigned, unsigned, unsigned);
