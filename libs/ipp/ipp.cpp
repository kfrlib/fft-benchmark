/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "ipp.h"
#include "fft_benchmark.hpp"
#include "src_benchmark.hpp"
#if __has_include("ipp/ipps.h")
#include "ipp/ipps.h"
#else
#include "ipps.h"
#endif
#include <string>

constexpr IppHintAlgorithm hint = ippAlgHintFast;

std::string fft_name()
{
    ippInit();

    const IppLibraryVersion* ver = ippsGetLibVersion();
    return std::string(ver->Name) + ver->Version;
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

    using Ipp32_T                            = pick_t<Ipp32fc, Ipp64fc>;
    using IppsDFTSpec_C_T                    = pick_t<IppsDFTSpec_C_32fc, IppsDFTSpec_C_64fc>;
    constexpr static auto ippsDFTFwd_CToC_T  = pick(ippsDFTFwd_CToC_32fc, ippsDFTFwd_CToC_64fc);
    constexpr static auto ippsDFTInv_CToC_T  = pick(ippsDFTInv_CToC_32fc, ippsDFTInv_CToC_64fc);
    constexpr static auto ippsDFTGetSize_C_T = pick(ippsDFTGetSize_C_32fc, ippsDFTGetSize_C_64fc);
    constexpr static auto ippsDFTInit_C_T    = pick(ippsDFTInit_C_32fc, ippsDFTInit_C_64fc);

    fft_implementation(sizes_t<1> size, real*, const real*)
    {
        int specsize = 0;
        int initsize = 0;
        int bufsize  = 0;
        ippsDFTGetSize_C_T(size[0], IPP_FFT_NODIV_BY_ANY, hint, &specsize, &initsize, &bufsize);
        initmem = initsize ? ippsMalloc_8u(initsize) : NULL;
        plan    = (IppsDFTSpec_C_T*)ippsMalloc_8u(specsize);
        ippsDFTInit_C_T(size[0], IPP_FFT_NODIV_BY_ANY, hint, plan, initmem);
        temp = bufsize ? ippsMalloc_8u(bufsize) : NULL;
    }

    void execute(real* out, const real* in)
    {
        if constexpr (invert)
        {
            ippsDFTInv_CToC_T((const Ipp32_T*)in, (Ipp32_T*)out, plan, temp);
        }
        else
        {
            ippsDFTFwd_CToC_T((const Ipp32_T*)in, (Ipp32_T*)out, plan, temp);
        }
    }
    ~fft_implementation()
    {
        ippsFree(initmem);
        ippsFree(temp);
        ippsFree(plan);
    }

private:
    IppsDFTSpec_C_T* plan;
    Ipp8u* temp;
    Ipp8u* initmem;
};

// real
// IPP ippsDFTFwd_RToCCS / ippsDFTInv_CCSToR use the IPP "CCS" packed layout,
// which is the benchmark's `ccs` layout: 2*(N/2+1) reals
//   even N: [Re0, Im0(=0), Re1, Im1, ..., Re(N/2-1), Im(N/2-1), Re(N/2), Im(N/2)(=0)]
//   odd  N: [Re0, Im0(=0), Re1, Im1, ..., Re((N-1)/2), Im((N-1)/2)]
// (Nyquist at index 2*half for even N; Im0 and ImNyq are structurally zero.)
// This is NOT the IPP "Perm" (half-complex, hc) layout. No conversion is
// performed — the benchmark adapts to the reported layout.
template <typename real, bool invert, bool inplace>
class fft_implementation<1, real, false, invert, inplace> : public fft_impl<real>
{
public:
    PICK;

    using IppsDFTSpec_R_T                     = pick_t<IppsDFTSpec_R_32f, IppsDFTSpec_R_64f>;
    constexpr static auto ippsDFTInv_CCSToR_T = pick(ippsDFTInv_CCSToR_32f, ippsDFTInv_CCSToR_64f);
    constexpr static auto ippsDFTFwd_RToCCS_T = pick(ippsDFTFwd_RToCCS_32f, ippsDFTFwd_RToCCS_64f);
    constexpr static auto ippsDFTGetSize_R_T  = pick(ippsDFTGetSize_R_32f, ippsDFTGetSize_R_64f);
    constexpr static auto ippsDFTInit_R_T     = pick(ippsDFTInit_R_32f, ippsDFTInit_R_64f);

    fft_implementation(sizes_t<1> size, real*, const real*)
    {
        int specsize = 0;
        int initsize = 0;
        int bufsize  = 0;
        ippsDFTGetSize_R_T(size[0], IPP_FFT_NODIV_BY_ANY, hint, &specsize, &initsize, &bufsize);
        initmem = initsize ? ippsMalloc_8u(initsize) : NULL;
        plan    = (IppsDFTSpec_R_T*)ippsMalloc_8u(specsize);
        ippsDFTInit_R_T(size[0], IPP_FFT_NODIV_BY_ANY, hint, plan, initmem);
        temp = bufsize ? ippsMalloc_8u(bufsize) : NULL;
    }

    real_layout layout() const final { return real_layout::ccs; }

    void execute(real* out, const real* in)
    {
        if constexpr (invert)
        {
            ippsDFTInv_CCSToR_T(in, out, plan, temp);
        }
        else
        {
            ippsDFTFwd_RToCCS_T(in, out, plan, temp);
        }
    }
    ~fft_implementation()
    {
        ippsFree(initmem);
        ippsFree(temp);
        ippsFree(plan);
    }

private:
    IppsDFTSpec_R_T* plan;
    Ipp8u* temp;
    Ipp8u* initmem;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, real* out, const real* in, bool is_complex,
                              bool invert, bool inplace)
{
    return fft_create_for<fft_implementation, real>(size, out, in, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, float*,
                                                            const float*, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, double*,
                                                              const double*, bool, bool, bool);

std::string src_name()
{
    ippInit();
    const IppLibraryVersion* ver = ippsGetLibVersion();
    return std::string(ver->Name) + ver->Version;
}

template <typename real>
struct src_implementation;

template <>
struct src_implementation<float> : public src_impl<float>
{
    src_implementation(unsigned out_rate, unsigned in_rate, unsigned seconds)
    {
        out_length = out_rate * seconds;
        in_length  = in_rate * seconds;

        int specSize         = 0;
        int filterHeight     = 0;
        const int filterSize = 2048; // Matches the filter size used by KFR's high preset
        const Ipp32f rollf   = 0.99f; // fraction of lower Nyquist where passband ends
        const Ipp32f alpha   = 14.47f; // Kaiser window parameter (~140 dB sidelobe attenuation)

        ippsResamplePolyphaseFixedGetSize_32f(in_rate, out_rate, filterSize, &specSize, &filterLen,
                                              &filterHeight, hint);
        spec = (IppsResamplingPolyphaseFixed_32f*)ippsMalloc_8u(specSize);
        ippsResamplePolyphaseFixedInit_32f(in_rate, out_rate, filterSize, rollf, alpha, spec, hint);
        // ippsResamplePolyphaseFixed_32f reads filterLen/2 samples before the time
        // position, so we must prepend history zeros to avoid out-of-bounds reads.
        history   = filterLen / 2;
        paddedLen = static_cast<int>(in_length) + filterLen + 2;
        paddedBuf = ippsMalloc_32f(paddedLen);
        ippsZero_32f(paddedBuf, paddedLen);
    }

    void execute(float* out, const float* in) final
    {
        // Copy input into padded buffer after the history region
        ippsCopy_32f(in, paddedBuf + history, static_cast<int>(in_length));

        Ipp64f time = history;
        int outlen  = 0;
        ippsResamplePolyphaseFixed_32f(paddedBuf, static_cast<int>(in_length), out, 1.0f, &time, &outlen,
                                       spec);
    }

    ~src_implementation()
    {
        if (paddedBuf)
            ippsFree(paddedBuf);
        if (spec)
            ippsFree(spec);
    }

    IppsResamplingPolyphaseFixed_32f* spec = nullptr;
    Ipp32f* paddedBuf                      = nullptr;
    int paddedLen;
    size_t out_length;
    size_t in_length;
    int filterLen;
    int history;
};

template <typename real>
src_impl_ptr<real> src_create(unsigned out_rate, unsigned in_rate, unsigned seconds)
{
    if constexpr (std::is_same_v<real, float>)
        return src_impl_ptr<float>(new src_implementation<float>(out_rate, in_rate, seconds));
    else
        return nullptr; // IPP resampling only supports 32f
}

template std::unique_ptr<src_impl<float>> src_create<float>(unsigned, unsigned, unsigned);
template std::unique_ptr<src_impl<double>> src_create<double>(unsigned, unsigned, unsigned);
