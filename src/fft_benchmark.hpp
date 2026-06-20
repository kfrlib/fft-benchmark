#pragma once

#include "benchmark.hpp"

using namespace bm;

#define PICK                                                                                                 \
    template <typename T1, typename T2>                                                                      \
    using pick_t = std::conditional_t<sizeof(real) == 4, T1, T2>;                                            \
    template <typename T1, typename T2>                                                                      \
    static constexpr auto pick(T1 v1, T2 v2)                                                                 \
    {                                                                                                        \
        if constexpr (sizeof(real) == 4)                                                                     \
            return v1;                                                                                       \
        else                                                                                                 \
            return v2;                                                                                       \
    }

template <size_t dims>
using sizes_t = std::array<size_t, dims>;

template <typename real>
void fill_random(real* in, size_t size)
{
    for (size_t i = 0; i < size; i++)
        in[i] = static_cast<real>(((double)rand() / RAND_MAX) * 2.0 - 1.0);
}

template <typename real>
static double rms(const real* a, const double* ref, size_t size)
{
    double sum = 0;
    for (size_t i = 0; i < size; i++)
    {
        double diff = a[i] - ref[i];
        sum += diff * diff;
    }
    return std::sqrt(sum / size);
}

// ---- Layout-agnostic real-spectrum comparison ------------------------------
//
// 1D real-transform packed layouts.
//
// A real DFT of length N has N/2+1 unique complex bins (k = 0..N/2). Libraries
// pack these bins into a real buffer using one of the following layouts. Each
// library wrapper reports its *preferred* layout via fft_impl::layout() and
// produces/consumes spectrum data only in that layout — wrappers must not
// perform any layout conversion (the user can adapt to any layout, so adding
// conversion in the wrapper would unfairly penalize the library). The accuracy
// check adapts to the reported layout and compares in a layout-agnostic way.
//
// The reference test vectors (dft_testvector_real_output*) are stored in the
// ccs layout: 2*(N/2+1) interleaved reals [Re0, Im0, Re1, Im1, ..., ReNyq, ImNyq].
// Library wrappers produce spectra in their own preferred layout. To compare
// fairly without forcing any wrapper to convert, we extract the (Re, Im) of
// each unique bin k = 0..N/2 from both the library output and the reference,
// then compute the RMS / max error over those bins. Bins that are structurally
// zero (Im0, ImNyq for even N) are skipped on both sides.

enum class real_layout
{
    // Interleaved complex, 2*(N/2+1) reals (FFTW / MKL-CCE / Sleef / OTFFT / KFR / JUCE):
    //   [Re0, Im0, Re1, Im1, ..., Re(N/2), Im(N/2)]   (Im0 = Im(N/2) = 0)
    ccs,
    // FFTPACK rfftf, N reals (pocketfft / IPP-Pack):
    //   even N: [Re0, Re1, Im1, ..., Re(N/2-1), Im(N/2-1), Re(N/2)]   (Nyquist at end)
    //   odd  N: [Re0, Re1, Im1, ..., Re((N-1)/2), Im((N-1)/2)]
    fftpack,
    // Half-complex / IPP-CCS, N reals (IPP-CCS / pffft):
    //   even N: [Re0, Re(N/2), Re1, Im1, ..., Re(N/2-1), Im(N/2-1)]   (Nyquist at index 1)
    //   odd  N: same as fftpack
    hc,
};

// Number of real elements a library needs to store the spectrum of a length-N
// real transform in the given layout.
inline size_t real_spectrum_size(size_t N, real_layout layout)
{
    switch (layout)
    {
    case real_layout::ccs:     return 2 * (N / 2 + 1);
    case real_layout::fftpack: return N;
    case real_layout::hc:      return N;
    }
    return 2 * (N / 2 + 1);
}

struct spectrum_error
{
    double rms;
    double max;
};

// Compare a library-produced packed spectrum (flat real array, `layout`) of a
// length-N real forward DFT against a ccs reference spectrum (`ref_ccs`, always
// 2*(N/2+1) reals). `scale` is applied to the reference. Returns RMS and max
// error over the unique bins (DC, Nyquist if even, and the complex bins).
template <typename real>
inline spectrum_error compare_spectrum(const real* lib, size_t N, real_layout layout,
                                       const double* ref_ccs, double scale)
{
    const size_t half = N / 2;
    const bool even   = (N % 2 == 0);
    const size_t nbins = even ? half - 1 : half; // complex bins k=1..nbins

    // Indices into the library's flat array.
    size_t lib_nyq, lib_bin1;
    switch (layout)
    {
    case real_layout::ccs:     lib_nyq = 2 * half; lib_bin1 = 2; break;
    case real_layout::fftpack: lib_nyq = N - 1;    lib_bin1 = 1; break;
    case real_layout::hc:      lib_nyq = 1;        lib_bin1 = 2; break;
    }

    double sum = 0, maxerr = 0;
    size_t count = 0;
    auto acc = [&](double lib_val, double ref_val)
    {
        double d = lib_val - ref_val * scale;
        sum += d * d;
        maxerr = std::max(maxerr, std::abs(d));
        ++count;
    };

    // DC (k=0): real only. ccs reference: ref_ccs[0].
    acc(static_cast<double>(lib[0]), ref_ccs[0]);

    // Nyquist (k=half): real only, present only for even N.
    // ccs reference: ref_ccs[2*half].
    if (even)
        acc(static_cast<double>(lib[lib_nyq]), ref_ccs[2 * half]);

    // Complex bins k=1..nbins: [r1,i1,r2,i2,...] contiguous in both layouts.
    // ccs reference: ref_ccs[2*k], ref_ccs[2*k+1].
    for (size_t k = 1; k <= nbins; ++k)
    {
        size_t li = lib_bin1 + 2 * (k - 1);
        acc(static_cast<double>(lib[li]),     ref_ccs[2 * k]);
        acc(static_cast<double>(lib[li + 1]), ref_ccs[2 * k + 1]);
    }

    spectrum_error e;
    e.rms = std::sqrt(sum / count);
    e.max = maxerr;
    return e;
}

// Convert a ccs reference spectrum (2*(N/2+1) reals) into a library's packed
// layout, for feeding a c2r inverse transform. `dst` must hold
// real_spectrum_size(N, layout) elements. No dynamic allocation.
template <typename T>
inline void convert_ccs_to_layout(const double* ccs, size_t N, real_layout layout, T* dst)
{
    const size_t half = N / 2;
    const bool even   = (N % 2 == 0);
    const size_t nbins = even ? half - 1 : half;

    size_t dst_nyq, dst_bin1;
    switch (layout)
    {
    case real_layout::ccs:     dst_nyq = 2 * half; dst_bin1 = 2; break;
    case real_layout::fftpack: dst_nyq = N - 1;    dst_bin1 = 1; break;
    case real_layout::hc:      dst_nyq = 1;        dst_bin1 = 2; break;
    }

    // DC
    dst[0] = static_cast<T>(ccs[0]);
    // Nyquist (even N only)
    if (even)
        dst[dst_nyq] = static_cast<T>(ccs[2 * half]);
    // Complex bins k=1..nbins
    for (size_t k = 1; k <= nbins; ++k)
    {
        size_t di = dst_bin1 + 2 * (k - 1);
        dst[di]     = static_cast<T>(ccs[2 * k]);
        dst[di + 1] = static_cast<T>(ccs[2 * k + 1]);
    }
}

inline size_t parse_number(std::string_view& s)
{
    size_t n = s.find_first_not_of("0123456789");
    if (n == 0)
        return 0;
    if (n == std::string_view::npos)
        n = s.size();
    size_t result;
    std::from_chars(s.data(), s.data() + n, result);
    s = s.substr(n);
    return result;
}

inline size_t product(std::vector<size_t> sizes)
{
    size_t result = sizes[0];
    for (size_t i = 1; i < sizes.size(); ++i)
        result *= sizes[i];
    return result;
}

inline std::string sizes_to_string(std::vector<size_t> sizes)
{
    std::string result;
    for (size_t n : sizes)
    {
        if (!result.empty())
            result += "x";

        char buf[32];
        size_t wr = std::snprintf(buf, sizeof(buf), "%zu", n);
        result += std::string_view(std::begin(buf), std::min(wr, sizeof(buf)));
    }
    return result;
}

inline std::vector<size_t> parse_size(std::string_view s)
{
    std::vector<size_t> result;
    if (s.empty())
        return result;
    while (size_t n = parse_number(s))
    {
        result.push_back(n);
        if (s.empty())
            return result;
        if (s[0] == 'x')
            s = s.substr(1);
        else
            return {};
    }
    return result;
}

inline std::vector<bool> to_vector_bool(std::string_view s)
{
    using namespace std::string_view_literals;

    std::vector<bool> result;
    for (char c : s)
    {
        if ("yY1"sv.find_first_of(c) != std::string_view::npos)
        {
            result.push_back(true);
        }
        else if ("nN0"sv.find_first_of(c) != std::string_view::npos)
        {
            result.push_back(false);
        }
    }
    return result;
}

extern bool avx2only;

// Normalization convention used by a library's transform.
//
// Wrappers must disable any library-side scaling when the library exposes a
// setting for it, and report the resulting convention here. The accuracy
// verifier applies the inverse scaling to the reference so that no rescaling
// is ever performed inside a wrapper.
enum class fft_scaling
{
    // No scaling at all: forward and inverse are both unnormalized, so
    //   inverse(forward(x)) == N * x.
    none,
    // Unitary / orthonormal: both forward and inverse scale by sqrt(1/N), so
    //   inverse(forward(x)) == x.
    sqrt_n,
    // Inverse scales by 1/N: forward is unnormalized, inverse divides by N, so
    //   inverse(forward(x)) == x.
    inverse_n,
    // vDSP convention: forward output is 2x the standard unnormalized DFT sum
    // (C=2), inverse is unnormalized (C=1), so inverse(forward(x)) == 2 * x.
    // This factor is inherent to the vDSP API and cannot be disabled.
    vdsp,
};

template <typename real>
class fft_impl
{
public:
    virtual ~fft_impl() {}
    virtual void execute(real* out, const real* in) = 0;

    // Preferred packed layout of the spectrum for real transforms. Complex
    // transforms ignore this. Defaults to ccs (the most common interleaved
    // layout); wrappers whose native real layout differs must override this.
    virtual real_layout layout() const { return real_layout::ccs; }

    // Normalization convention of the transform. Defaults to none (forward and
    // inverse both unnormalized). Wrappers must disable library-side scaling
    // when possible and report the convention actually in effect.
    virtual fft_scaling scaling() const { return fft_scaling::none; }

    constexpr static bool valid = true;
};

class fft_impl_stub
{
public:
    constexpr static bool valid = false;
};

template <typename real>
using fft_impl_ptr = std::unique_ptr<fft_impl<real>>;

// Forward declarations
template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool inverse, bool inplace);

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims,
          bool is_complex, bool invert, bool inplace>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, std::integral_constant<bool, is_complex>,
                                  std::integral_constant<bool, invert>, std::integral_constant<bool, inplace>)
{
    if constexpr (fft_implementation<dims, real, is_complex, invert, inplace>::valid)
    {
        return fft_impl_ptr<real>(new fft_implementation<dims, real, is_complex, invert, inplace>(size));
    }
    return nullptr;
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims,
          bool is_complex, bool invert>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, std::integral_constant<bool, is_complex>,
                                  std::integral_constant<bool, invert>, bool inplace)
{
    if (inplace)
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, invert>{},
                                                        std::integral_constant<bool, true>{});
    else
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, invert>{},
                                                        std::integral_constant<bool, false>{});
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims,
          bool is_complex>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, std::integral_constant<bool, is_complex>,
                                  bool invert, bool inplace)
{
    if (invert)
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, true>{}, inplace);
    else
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, is_complex>{},
                                                        std::integral_constant<bool, false>{}, inplace);
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real, size_t dims>
fft_impl_ptr<real> fft_create_for(const sizes_t<dims>& size, bool is_complex, bool invert, bool inplace)
{
    if (is_complex)
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, true>{}, invert,
                                                        inplace);
    else
        return fft_create_for<fft_implementation, real>(size, std::integral_constant<bool, false>{}, invert,
                                                        inplace);
}

template <template <int, typename, bool, bool, bool> typename fft_implementation, typename real>
fft_impl_ptr<real> fft_create_for(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    if (size.size() == 1)
        return fft_create_for<fft_implementation, real>(sizes_t<1>{ size[0] }, is_complex, invert, inplace);
    if (size.size() == 2)
        return fft_create_for<fft_implementation, real>(sizes_t<2>{ size[0], size[1] }, is_complex, invert,
                                                        inplace);
    if (size.size() == 3)
        return fft_create_for<fft_implementation, real>(sizes_t<3>{ size[0], size[1], size[2] }, is_complex,
                                                        invert, inplace);
    return nullptr;
}

extern template fft_impl_ptr<float> fft_create<float>(const std::vector<size_t>& size, bool is_complex,
                                                      bool inverse, bool inplace);
extern template fft_impl_ptr<double> fft_create<double>(const std::vector<size_t>& size, bool is_complex,
                                                        bool inverse, bool inplace);

std::string fft_name();
