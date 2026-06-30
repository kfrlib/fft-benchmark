/**
 * FFT bencmarking tool (http://kfrlib.com)
 * Copyright (C) 2016-2026 Dan Casarin
 * Benchmark source code is MIT-licensed
 * See LICENSE.txt for details
 */

#include "fft_benchmark.hpp"
#include <Accelerate/Accelerate.h>
#include <memory>
#include <string>

std::string fft_name() { return "vDSP (Accelerate)"; }

// Default stub: unsupported configuration
template <int Dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl_stub
{
};

// 1D complex DFT.
// vDSP_DFT_Interleaved_CreateSetup supports sizes up to 4096; for larger sizes
// we fall back to vDSP_DFT_zop_CreateSetup (split-complex) with fast
// vDSP_ctoz / vDSP_ztoc interleave/deinterleave conversions.
template <typename real, bool invert, bool inplace>
class fft_implementation<1, real, true, invert, inplace> : public fft_impl<real>
{
public:
    PICK;

    using InterleavedSetup = pick_t<vDSP_DFT_Interleaved_Setup, vDSP_DFT_Interleaved_SetupD>;
    using ZopSetup         = pick_t<vDSP_DFT_Setup, vDSP_DFT_SetupD>;

    fft_implementation(sizes_t<1> sizes, real*, const real*) : N(sizes[0])
    {
        constexpr vDSP_DFT_Direction dir = invert ? vDSP_DFT_INVERSE : vDSP_DFT_FORWARD;

        // Try the interleaved API first (works up to N=4096)
        if constexpr (sizeof(real) == 4)
            isetup = vDSP_DFT_Interleaved_CreateSetup(nullptr, static_cast<vDSP_Length>(N), dir,
                                                      vDSP_DFT_Interleaved_ComplextoComplex);
        else
            isetup = vDSP_DFT_Interleaved_CreateSetupD(nullptr, static_cast<vDSP_Length>(N), dir,
                                                       vDSP_DFT_Interleaved_ComplextoComplex);

        if (!isetup)
        {
            // Fall back to split-complex DFT (supports all benchmark sizes)
            if constexpr (sizeof(real) == 4)
                zsetup = vDSP_DFT_zop_CreateSetup(nullptr, static_cast<vDSP_Length>(N), dir);
            else
                zsetup = vDSP_DFT_zop_CreateSetupD(nullptr, static_cast<vDSP_Length>(N), dir);

            if (zsetup)
            {
                // Two scratch split-complex buffers: one for input deinterleave,
                // one for output reinterleave (handles in==out correctly).
                split_re_in  = aligned_malloc<real>(N);
                split_im_in  = aligned_malloc<real>(N);
                split_re_out = aligned_malloc<real>(N);
                split_im_out = aligned_malloc<real>(N);
            }
        }
    }

    bool valid_setup() const { return isetup != nullptr || zsetup != nullptr; }

    // vDSP's complex DFT (Interleaved and zop) is unnormalized in both
    // directions: forward produces the standard DFT sum (no 2x factor), and
    // inverse is also unnormalized. The 2x factor only applies to the real
    // zrop forward transform, so the complex path reports `none` here.
    fft_scaling scaling() const final { return fft_scaling::none; }

    void execute(real* out, const real* in)
    {
        if (isetup)
        {
            // Direct interleaved path — zero-copy
            if constexpr (sizeof(real) == 4)
                vDSP_DFT_Interleaved_Execute(isetup, reinterpret_cast<const DSPComplex*>(in),
                                             reinterpret_cast<DSPComplex*>(out));
            else
                vDSP_DFT_Interleaved_ExecuteD(isetup, reinterpret_cast<const DSPDoubleComplex*>(in),
                                              reinterpret_cast<DSPDoubleComplex*>(out));
        }
        else
        {
            // Split-complex path: deinterleave → DFT → reinterleave
            if constexpr (sizeof(real) == 4)
            {
                DSPSplitComplex si = { split_re_in, split_im_in };
                DSPSplitComplex so = { split_re_out, split_im_out };
                vDSP_ctoz(reinterpret_cast<const DSPComplex*>(in), 2, &si, 1, N);
                vDSP_DFT_Execute(zsetup, split_re_in, split_im_in, split_re_out, split_im_out);
                vDSP_ztoc(&so, 1, reinterpret_cast<DSPComplex*>(out), 2, N);
            }
            else
            {
                DSPDoubleSplitComplex si = { split_re_in, split_im_in };
                DSPDoubleSplitComplex so = { split_re_out, split_im_out };
                vDSP_ctozD(reinterpret_cast<const DSPDoubleComplex*>(in), 2, &si, 1, N);
                vDSP_DFT_ExecuteD(zsetup, split_re_in, split_im_in, split_re_out, split_im_out);
                vDSP_ztocD(&so, 1, reinterpret_cast<DSPDoubleComplex*>(out), 2, N);
            }
        }
    }

    ~fft_implementation()
    {
        if (isetup)
        {
            if constexpr (sizeof(real) == 4)
                vDSP_DFT_Interleaved_DestroySetup(isetup);
            else
                vDSP_DFT_Interleaved_DestroySetupD(isetup);
        }
        if (zsetup)
        {
            if constexpr (sizeof(real) == 4)
                vDSP_DFT_DestroySetup(zsetup);
            else
                vDSP_DFT_DestroySetupD(zsetup);
            aligned_free(split_re_in, N);
            aligned_free(split_im_in, N);
            aligned_free(split_re_out, N);
            aligned_free(split_im_out, N);
        }
    }

private:
    size_t N;
    InterleavedSetup isetup = nullptr;
    ZopSetup zsetup         = nullptr;
    real* split_re_in       = nullptr;
    real* split_im_in       = nullptr;
    real* split_re_out      = nullptr;
    real* split_im_out      = nullptr;
};

// 1D real-to-complex (forward) DFT via vDSP_DFT_zrop_CreateSetup.
//
// vDSP zrop forward layout (N/2 split-complex elements):
//   input:  Ir[j] = in[2j], Ii[j] = in[2j+1]  (packed real signal)
//   output: Or[0]  = H[0]   (DC, pure real)
//           Oi[0]  = H[N/2] (Nyquist, pure real)
//           Or[k]  = Re(H[k]), Oi[k] = Im(H[k])  for 1 <= k < N/2
//   scale:  vDSP forward has C=2 (output is 2x the standard unnormalized DFT).
//           No scaling is performed in the wrapper; the accuracy verifier
//           adapts to the library's reported convention.
//
// Benchmark output format: N/2+1 interleaved complex (FFTW style).
template <typename real, bool inplace>
class fft_implementation<1, real, false, false, inplace> : public fft_impl<real>
{
public:
    PICK;
    using ZropSetup = pick_t<vDSP_DFT_Setup, vDSP_DFT_SetupD>;

    fft_implementation(sizes_t<1> sizes, real*, const real*) : N(sizes[0])
    {
        if constexpr (sizeof(real) == 4)
            setup = vDSP_DFT_zrop_CreateSetup(nullptr, static_cast<vDSP_Length>(N), vDSP_DFT_FORWARD);
        else
            setup = vDSP_DFT_zrop_CreateSetupD(nullptr, static_cast<vDSP_Length>(N), vDSP_DFT_FORWARD);

        if (setup)
        {
            Ir = aligned_malloc<real>(N / 2);
            Ii = aligned_malloc<real>(N / 2);
            Or = aligned_malloc<real>(N / 2);
            Oi = aligned_malloc<real>(N / 2);
        }
    }

    bool valid_setup() const { return setup != nullptr; }

    fft_scaling scaling() const final { return fft_scaling::vdsp; }

    void execute(real* out, const real* in)
    {
        // Deinterleave N reals → N/2 split-complex (reuses ctoz)
        if constexpr (sizeof(real) == 4)
        {
            DSPSplitComplex si = { Ir, Ii };
            vDSP_ctoz(reinterpret_cast<const DSPComplex*>(in), 2, &si, 1, N / 2);
            vDSP_DFT_Execute(setup, Ir, Ii, Or, Oi);

            // Repack to FFTW-style N/2+1 interleaved complex:
            //   [DC_re, 0, bin1_re, bin1_im, ..., Nyq_re, 0]
            out[0]             = Or[0];
            out[1]             = 0.0f;
            DSPSplitComplex so = { Or + 1, Oi + 1 };
            vDSP_ztoc(&so, 1, reinterpret_cast<DSPComplex*>(out + 2), 2, N / 2 - 1);
            out[N]     = Oi[0];
            out[N + 1] = 0.0f;
        }
        else
        {
            DSPDoubleSplitComplex si = { Ir, Ii };
            vDSP_ctozD(reinterpret_cast<const DSPDoubleComplex*>(in), 2, &si, 1, N / 2);
            vDSP_DFT_ExecuteD(setup, Ir, Ii, Or, Oi);

            out[0]                   = Or[0];
            out[1]                   = 0.0;
            DSPDoubleSplitComplex so = { Or + 1, Oi + 1 };
            vDSP_ztocD(&so, 1, reinterpret_cast<DSPDoubleComplex*>(out + 2), 2, N / 2 - 1);
            out[N]     = Oi[0];
            out[N + 1] = 0.0;
        }
    }

    ~fft_implementation()
    {
        if (setup)
        {
            if constexpr (sizeof(real) == 4)
                vDSP_DFT_DestroySetup(setup);
            else
                vDSP_DFT_DestroySetupD(setup);
            aligned_free(Ir, N / 2);
            aligned_free(Ii, N / 2);
            aligned_free(Or, N / 2);
            aligned_free(Oi, N / 2);
        }
    }

private:
    size_t N;
    ZropSetup setup = nullptr;
    real *Ir = nullptr, *Ii = nullptr, *Or = nullptr, *Oi = nullptr;
};

// 1D complex-to-real (inverse) DFT via vDSP_DFT_zrop_CreateSetup.
//
// Benchmark input format: N/2+1 interleaved complex (FFTW style).
// vDSP zrop inverse layout: same split-complex as forward but direction swapped.
// No output scaling needed (vDSP inverse has C=1).
template <typename real, bool inplace>
class fft_implementation<1, real, false, true, inplace> : public fft_impl<real>
{
public:
    PICK;
    using ZropSetup = pick_t<vDSP_DFT_Setup, vDSP_DFT_SetupD>;

    fft_implementation(sizes_t<1> sizes, real*, const real*) : N(sizes[0])
    {
        if constexpr (sizeof(real) == 4)
            setup = vDSP_DFT_zrop_CreateSetup(nullptr, static_cast<vDSP_Length>(N), vDSP_DFT_INVERSE);
        else
            setup = vDSP_DFT_zrop_CreateSetupD(nullptr, static_cast<vDSP_Length>(N), vDSP_DFT_INVERSE);

        if (setup)
        {
            Ir = aligned_malloc<real>(N / 2);
            Ii = aligned_malloc<real>(N / 2);
            Or = aligned_malloc<real>(N / 2);
            Oi = aligned_malloc<real>(N / 2);
        }
    }

    bool valid_setup() const { return setup != nullptr; }

    fft_scaling scaling() const final { return fft_scaling::vdsp; }

    void execute(real* out, const real* in)
    {
        // Unpack FFTW-style N/2+1 interleaved complex → vDSP zrop split layout:
        //   Ir[0] = in[0]  (DC real),  Ii[0] = in[N] (Nyquist real)
        //   Ir[k] = in[2k], Ii[k] = in[2k+1]  for k=1..N/2-1
        if constexpr (sizeof(real) == 4)
        {
            Ir[0]              = in[0];
            Ii[0]              = in[N];
            DSPSplitComplex si = { Ir + 1, Ii + 1 };
            vDSP_ctoz(reinterpret_cast<const DSPComplex*>(in + 2), 2, &si, 1, N / 2 - 1);
            vDSP_DFT_Execute(setup, Ir, Ii, Or, Oi);

            // Reinterleave N/2 split-complex output → N reals
            DSPSplitComplex so = { Or, Oi };
            vDSP_ztoc(&so, 1, reinterpret_cast<DSPComplex*>(out), 2, N / 2);
        }
        else
        {
            Ir[0]                    = in[0];
            Ii[0]                    = in[N];
            DSPDoubleSplitComplex si = { Ir + 1, Ii + 1 };
            vDSP_ctozD(reinterpret_cast<const DSPDoubleComplex*>(in + 2), 2, &si, 1, N / 2 - 1);
            vDSP_DFT_ExecuteD(setup, Ir, Ii, Or, Oi);

            DSPDoubleSplitComplex so = { Or, Oi };
            vDSP_ztocD(&so, 1, reinterpret_cast<DSPDoubleComplex*>(out), 2, N / 2);
        }
    }

    ~fft_implementation()
    {
        if (setup)
        {
            if constexpr (sizeof(real) == 4)
                vDSP_DFT_DestroySetup(setup);
            else
                vDSP_DFT_DestroySetupD(setup);
            aligned_free(Ir, N / 2);
            aligned_free(Ii, N / 2);
            aligned_free(Or, N / 2);
            aligned_free(Oi, N / 2);
        }
    }

private:
    size_t N;
    ZropSetup setup = nullptr;
    real *Ir = nullptr, *Ii = nullptr, *Or = nullptr, *Oi = nullptr;
};

// Helper: construct, validate, and wrap an fft_implementation
template <typename Impl, typename real>
static fft_impl_ptr<real> make_if_valid(Impl* impl)
{
    if (!impl->valid_setup())
    {
        delete impl;
        return nullptr;
    }
    return fft_impl_ptr<real>(impl);
}

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, real* out, const real* in, bool is_complex,
                              bool invert, bool inplace)
{
    if (size.size() != 1)
        return nullptr;
    const size_t N = size[0];

    if (is_complex)
    {
        // Complex DFT: interleaved API (≤4096) with zop fallback
        using F = fft_implementation<1, real, true, false, false>;
        using I = fft_implementation<1, real, true, true, false>;
        if (invert)
            return make_if_valid<I, real>(new I({ N }, out, in));
        else
            return make_if_valid<F, real>(new F({ N }, out, in));
    }
    else
    {
        // Real transforms require even N (vDSP zrop requirement)
        if (N < 2 || (N & 1))
            return nullptr;

        if (!invert)
        {
            // R2C forward
            using F = fft_implementation<1, real, false, false, false>;
            return make_if_valid<F, real>(new F({ N }, out, in));
        }
        else
        {
            // C2R inverse
            using I = fft_implementation<1, real, false, true, false>;
            return make_if_valid<I, real>(new I({ N }, out, in));
        }
    }
}

template fft_impl_ptr<float> fft_create<float>(const std::vector<size_t>&, float*, const float*, bool, bool,
                                               bool);
template fft_impl_ptr<double> fft_create<double>(const std::vector<size_t>&, double*, const double*, bool,
                                                 bool, bool);
