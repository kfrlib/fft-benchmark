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

inline bool init_ipp() { return ippInit() == ippStsOk;}
inline void init() { static bool ok = init_ipp(); }

#ifdef TYPE_FLOAT

template <bool invert>
class fft_benchmark
{
public:
    fft_benchmark(size_t size, real* out, const real* in) : out(out), in(in)
    {
        init();
        ippsDFTInitAlloc_C_32fc(&plan, size, IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate);
        int bufsize = 0;
        ippsDFTGetBufSize_C_32fc(plan, &bufsize);
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
            ippsDFTInv_CToC_32fc((const Ipp32fc*)this->out, (Ipp32fc*)this->out, plan, temp);
        else
            ippsDFTFwd_CToC_32fc((const Ipp32fc*)this->out, (Ipp32fc*)this->out, plan, temp);
    }
    ~fft_benchmark()
    {
        ippsDFTFree_C_32fc(plan);
        ippsFree(temp);
    }

private:
    IppsDFTSpec_C_32fc* plan;
    real* out;
    const real* in;
    Ipp8u* temp;
};

#else

template <bool invert>
class fft_benchmark
{
public:
    fft_benchmark(size_t size, real* out, const real* in) : out(out), in(in)
    {
        init();
        ippsDFTInitAlloc_C_64fc(&plan, size, IPP_FFT_NODIV_BY_ANY, ippAlgHintAccurate);
        int bufsize = 0;
        ippsDFTGetBufSize_C_64fc(plan, &bufsize);
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
            ippsDFTInv_CToC_64fc((const Ipp64fc*)this->out, (Ipp64fc*)this->out, plan, temp);
        else
            ippsDFTFwd_CToC_64fc((const Ipp64fc*)this->out, (Ipp64fc*)this->out, plan, temp);
    }
    ~fft_benchmark()
    {
        ippsDFTFree_C_64fc(plan);
        ippsFree(temp);
    }

private:
    IppsDFTSpec_C_64fc* plan;
    real* out;
    const real* in;
    Ipp8u* temp;
};
#endif
