#pragma once

#include "benchmark.hpp"

template <typename real>
class fft_impl
{
public:
    virtual ~fft_impl() {}
    virtual void execute(real* out, const real* in) = 0;

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