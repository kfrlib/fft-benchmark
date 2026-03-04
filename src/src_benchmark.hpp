#pragma once

#include "benchmark.hpp"

template <typename real>
class src_impl
{
public:
    virtual ~src_impl() {}
    virtual void execute(real* out, const real* in) = 0;

    constexpr static bool valid = true;
};

class src_impl_stub
{
public:
    constexpr static bool valid = false;
};

template <typename real>
using src_impl_ptr = std::unique_ptr<src_impl<real>>;

// Forward declarations
template <typename real>
src_impl_ptr<real> src_create(unsigned out_rate, unsigned in_rate, unsigned length);

extern template src_impl_ptr<float> src_create<float>(unsigned out_rate, unsigned in_rate, unsigned length);
extern template src_impl_ptr<double> src_create<double>(unsigned out_rate, unsigned in_rate, unsigned length);

std::string src_name();
