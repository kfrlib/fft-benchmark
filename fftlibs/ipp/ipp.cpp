/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2023 Dan Cazarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "ipp.h"
#include "benchmark.hpp"
#if __has_include("ipp/ipps.h")
#include "ipp/ipps.h"
#else
#include "ipps.h"
#endif
#include <string>

std::string fft_name()
{
    ippInit();
#if defined(__x86_64__) || defined(_M_X64)
    if (avx2only)
    {
        fprintf(stderr, "IPP: enabling AVX2\n");
        ippSetCpuFeatures(ippCPUID_MMX | ippCPUID_SSE | ippCPUID_SSE2 | ippCPUID_SSE3 | ippCPUID_SSSE3 |
                          ippCPUID_MOVBE | ippCPUID_SSE41 | ippCPUID_SSE42 | ippCPUID_AES | ippCPUID_CLMUL |
                          ippCPUID_SHA | ippCPUID_AVX | ippAVX_ENABLEDBYOS | ippCPUID_RDRAND | ippCPUID_F16C |
                          ippCPUID_AVX2 | ippCPUID_MOVBE | ippCPUID_ADCOX | ippCPUID_RDSEED |
                          ippCPUID_PREFETCHW);
    }
#endif

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

    fft_implementation(sizes_t<1> size)
    {
        int specsize = 0;
        int initsize = 0;
        int bufsize  = 0;
        ippsDFTGetSize_C_T(size[0], IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate, &specsize, &initsize, &bufsize);
        initmem = initsize ? ippsMalloc_8u(initsize) : NULL;
        plan    = (IppsDFTSpec_C_T*)ippsMalloc_8u(specsize);
        ippsDFTInit_C_T(size[0], IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate, plan, initmem);
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
template <typename real, bool invert, bool inplace>
class fft_implementation<1, real, false, invert, inplace> : public fft_impl<real>
{
public:
    PICK;

    using IppsDFTSpec_R_T                      = pick_t<IppsDFTSpec_R_32f, IppsDFTSpec_R_64f>;
    constexpr static auto ippsDFTInv_PermToR_T = pick(ippsDFTInv_PermToR_32f, ippsDFTInv_PermToR_64f);
    constexpr static auto ippsDFTFwd_RToPerm_T = pick(ippsDFTFwd_RToPerm_32f, ippsDFTFwd_RToPerm_64f);
    constexpr static auto ippsDFTGetSize_R_T   = pick(ippsDFTGetSize_R_32f, ippsDFTGetSize_R_64f);
    constexpr static auto ippsDFTInit_R_T      = pick(ippsDFTInit_R_32f, ippsDFTInit_R_64f);

    fft_implementation(sizes_t<1> size)
    {
        int specsize = 0;
        int initsize = 0;
        int bufsize  = 0;
        ippsDFTGetSize_R_T(size[0], IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate, &specsize, &initsize, &bufsize);
        initmem = initsize ? ippsMalloc_8u(initsize) : NULL;
        plan    = (IppsDFTSpec_R_T*)ippsMalloc_8u(specsize);
        ippsDFTInit_R_T(size[0], IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate, plan, initmem);
        temp = bufsize ? ippsMalloc_8u(bufsize) : NULL;
    }

    void execute(real* out, const real* in)
    {
        if constexpr (invert)
        {
            ippsDFTInv_PermToR_T(in, out, plan, temp);
        }
        else
        {
            ippsDFTFwd_RToPerm_T(in, out, plan, temp);
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
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
