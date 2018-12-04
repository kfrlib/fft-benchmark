/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016 D Levin
 * Benchmark source code is dual-licensed under MIT and GPL 2 or later
 * See LICENSE.txt for details
 */
#pragma once

#define __int64 long long

#include "benchmark.hpp"
#include "ipp/ipp.h"
#include "ipp/ipps.h"
#include <string>

inline bool init_ipp() { return ippInit() == ippStsOk; }
inline void init() { static bool ok = init_ipp(); }

#ifdef TYPE_FLOAT
#define IPPSUFFIX(name) name##32fc
#else
#define IPPSUFFIX(name) name##64fc
#endif

template <bool invert>
class fft_benchmark
{
public:
    fft_benchmark(size_t size, real* out, const real* in) : out(out), in(in)
    {
        init();
//        ippSetCpuFeatures(ippCPUID_MMX | ippCPUID_SSE | ippCPUID_SSE2 | ippCPUID_SSE3 | ippCPUID_SSSE3 |
//                          ippCPUID_MOVBE | ippCPUID_SSE41 | ippCPUID_SSE42 | ippCPUID_AES | ippCPUID_CLMUL |
//                          ippCPUID_SHA | ippCPUID_AVX | ippAVX_ENABLEDBYOS | ippCPUID_RDRAND | ippCPUID_F16C |
//                          ippCPUID_AVX2 | ippCPUID_MOVBE | ippCPUID_ADCOX | ippCPUID_RDSEED |
//                          ippCPUID_PREFETCHW
//        );

        int specsize = 0;
        int initsize = 0;
        int bufsize  = 0;
        IPPSUFFIX(ippsDFTGetSize_C_)
        (size, IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate, &specsize, &initsize, &bufsize);
        initmem = initsize ? ippsMalloc_8u(initsize) : NULL;
        plan    = (IPPSUFFIX(IppsDFTSpec_C_)*)ippsMalloc_8u(specsize);
        IPPSUFFIX(ippsDFTInit_C_)(size, IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate, plan, initmem);
        temp = bufsize ? ippsMalloc_8u(bufsize) : NULL;
    }
    static std::string name()
    {
        init();
        const IppLibraryVersion* ver = ippsGetLibVersion();
        return std::string(ver->Name) + ver->Version;
    }
    static std::string shortname() { return "IPP"; }
    void execute()
    {
        if (invert)
            IPPSUFFIX(ippsDFTInv_CToC_)
        ((const IPPSUFFIX(Ipp)*)this->out, (IPPSUFFIX(Ipp)*)this->out, plan, temp);
        else IPPSUFFIX(ippsDFTFwd_CToC_)((const IPPSUFFIX(Ipp)*)this->out, (IPPSUFFIX(Ipp)*)this->out, plan,
                                         temp);
    }
    ~fft_benchmark()
    {
        ippsFree(initmem);
        ippsFree(temp);
        ippsFree(plan);
    }

private:
    IPPSUFFIX(IppsDFTSpec_C_) * plan;
    real* out;
    const real* in;
    Ipp8u* temp;
    Ipp8u* initmem;
};
