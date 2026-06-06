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

template <typename T>
constexpr inline const char* type_name = "float";

template <>
constexpr inline const char* type_name<double> = "double";

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

inline std::string execfile(std::string command)
{
    size_t pos = command.find_last_of("/\\");
    command    = command.substr(pos == std::string::npos ? 0 : pos + 1);
    if (command.substr(command.size() - 4) == ".exe")
        command = command.substr(0, command.size() - 4);
    return command;
}

extern bool avx2only;

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
