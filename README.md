# FFT benchmark
A benchmark for comparison of FFT algorithms performance.

Currently supports Intel IPP, [KFR](https://github.com/kfrlib/kfr), FFTW and KissFFT.

Requires:
* Clang 6.0+ (GCC and MSVC are not suitable)
* Latest MSYS2 on Windows
* CMake 3.0 or newer
* AVX2-capable cpu
* Python 3.5 or newer
  * matplotlib module
  * numpy module

## Usage

Place headers to `include` folder:
```
include/
    fft/
        fftw3.h
    ipp/
        ipp.h
        <other IPP headers>
    kfr/
        dft.hpp        
        <other KFR headers from include/kfr>
```
x64 libs to `lib` folder:
```
lib/
    ipps.lib
    <other IPP libs>
```
x64 dlls to `bin` folder, including MinGW system dlls:
```
bin/
    ipps.dll
    <other IPP dlls>
    ...
    libfftw3-3.dll
    libfftw3f-3.dll
    ...
    libstdc++-6.dll
    libwinpthread-1.dll
    libgcc_s_seh-1.dll

```

Run msys2-build.cmd to build.

Run benchmark.py to measure performance and save data/images.

## License

Dual licensed under GPL 2+ and MIT
