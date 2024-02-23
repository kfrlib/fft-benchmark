
#include "benchmark.hpp"

#include <mkl.h>
#include <mkl_dfti.h>

std::string fft_name()
{
#if defined(__x86_64__) || defined(_M_X64)
    MKL_Enable_Instructions(MKL_ENABLE_AVX2);
#endif
    char buf[1024];
    MKL_Get_Version_String(buf, sizeof(buf));
    return std::string(buf);
}

template <int dims, typename real, bool is_complex, bool invert, bool inplace>
class fft_implementation : public fft_impl<real>
{
public:
    PICK;

    constexpr static bool valid = is_complex || !inplace;

    static void handle_mkl_status(MKL_LONG status, const char* code)
    {
        if (status != 0)
        {
            const char* msg = DftiErrorMessage(status);
            fprintf(stderr, "MKL error: %s returned %lld (%s)\ndims=%d type=%s cmplx=%d inv=%d\n", code,
                    int64_t(status), msg, dims, type_name<real>, is_complex, invert);
            std::abort();
        }
    }

#define HANDLE_MKL_STATUS(...) handle_mkl_status(__VA_ARGS__, #__VA_ARGS__)

    constexpr static DFTI_CONFIG_VALUE precision = pick(DFTI_SINGLE, DFTI_DOUBLE);

    static MKL_LONG create(DFTI_DESCRIPTOR_HANDLE& handle, sizes_t<dims> size)
    {
        if constexpr (dims == 1)
        {
            return DftiCreateDescriptor(&handle, precision, is_complex ? DFTI_COMPLEX : DFTI_REAL, 1,
                                        size[0]);
        }
        else if constexpr (dims == 2)
        {
            const MKL_LONG sizes[2] = { static_cast<MKL_LONG>(size[0]), static_cast<MKL_LONG>(size[1]) };
            return DftiCreateDescriptor(&handle, precision, is_complex ? DFTI_COMPLEX : DFTI_REAL, 2, &sizes);
        }
        else if constexpr (dims == 3)
        {
            const MKL_LONG sizes[3] = { static_cast<MKL_LONG>(size[0]), static_cast<MKL_LONG>(size[1]),
                                        static_cast<MKL_LONG>(size[2]) };
            return DftiCreateDescriptor(&handle, precision, is_complex ? DFTI_COMPLEX : DFTI_REAL, 3, &sizes);
        }
    }

    fft_implementation(sizes_t<dims> size)
    {
        HANDLE_MKL_STATUS(create(handle_o, size));
        HANDLE_MKL_STATUS(DftiSetValue(handle_o, DFTI_PLACEMENT, DFTI_NOT_INPLACE));
        HANDLE_MKL_STATUS(DftiSetValue(handle_o, DFTI_CONJUGATE_EVEN_STORAGE, DFTI_COMPLEX_COMPLEX));
        HANDLE_MKL_STATUS(DftiSetValue(handle_o, DFTI_PACKED_FORMAT, DFTI_CCE_FORMAT));
        HANDLE_MKL_STATUS(DftiCommitDescriptor(handle_o));

        if (is_complex)
        {
            HANDLE_MKL_STATUS(create(handle_i, size));
            HANDLE_MKL_STATUS(DftiSetValue(handle_i, DFTI_PLACEMENT, DFTI_INPLACE));
            HANDLE_MKL_STATUS(DftiSetValue(handle_i, DFTI_CONJUGATE_EVEN_STORAGE, DFTI_COMPLEX_COMPLEX));
            HANDLE_MKL_STATUS(DftiSetValue(handle_i, DFTI_PACKED_FORMAT, DFTI_CCE_FORMAT));
            HANDLE_MKL_STATUS(DftiCommitDescriptor(handle_i));
        }
    }
    void execute(real* out, const real* in) final
    {
        if constexpr (invert)
        {
            if (in == out)
            {
                HANDLE_MKL_STATUS(DftiComputeBackward(handle_i, const_cast<real*>(in), out));
            }
            else
            {
                HANDLE_MKL_STATUS(DftiComputeBackward(handle_o, const_cast<real*>(in), out));
            }
        }
        else
        {
            if (in == out)
            {
                HANDLE_MKL_STATUS(DftiComputeForward(handle_i, const_cast<real*>(in), out));
            }
            else
            {
                HANDLE_MKL_STATUS(DftiComputeForward(handle_o, const_cast<real*>(in), out));
            }
        }
    }

    ~fft_implementation()
    {
        // HANDLE_MKL_STATUS(DftiFreeDescriptor(&handle_i));
        HANDLE_MKL_STATUS(DftiFreeDescriptor(&handle_o));
    }

private:
    DFTI_DESCRIPTOR_HANDLE handle_i;
    DFTI_DESCRIPTOR_HANDLE handle_o;
};

template <typename real>
fft_impl_ptr<real> fft_create(const std::vector<size_t>& size, bool is_complex, bool invert, bool inplace)
{
    return fft_create_for<fft_implementation, real>(size, is_complex, invert, inplace);
}

template std::unique_ptr<fft_impl<float>> fft_create<float>(const std::vector<size_t>&, bool, bool, bool);
template std::unique_ptr<fft_impl<double>> fft_create<double>(const std::vector<size_t>&, bool, bool, bool);
