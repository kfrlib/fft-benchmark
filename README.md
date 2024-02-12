# FFT benchmark
A benchmark for comparison of FFT algorithms performance.

Currently supports 
* [KFR](https://github.com/kfrlib/kfr)
* Intel IPP
* Intel MKL
* FFTW

`CMAKE_PREFIX_PATH` should contain the paths to cmake configs of used libraries.
Example:
```
C:/vcpkg/installed/x64-windows-static-md/share
C:/Program Files (x86)/Intel/oneAPI/ipp/2021.9.0/lib/cmake/ipp
C:/Program Files (x86)/Intel/oneAPI/mkl/2024.0/lib/cmake/mkl
kfr-install-dir/lib/cmake
```

Requires:
* Clang 12.0+
* CMake 3.12 or newer
* AVX2-capable cpu
* Python 3.5 or newer
  * matplotlib module
  * numpy module

## Benchmark code license

MIT
