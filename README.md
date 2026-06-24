# FFT benchmark

A benchmark for comparison of FFT algorithms performance.
Supports 1D, 2D, and 3D transforms with float and double precision.
Measures the performance of real/complex, in-place/out-of-place, forward/inverse FFT.

## Supported libraries

| Library                                                                                 |  Version   | Float | Double | 1D  | 2D & 3D | Arbitrary sizes | Notes                              | Runtime dispatch | Installation |
|-----------------------------------------------------------------------------------------|:----------:|:-----:|:------:|:---:|:-------:|:---------------:|------------------------------------|:----------------:|--------------|
| [KFR](https://github.com/kfrlib/kfr)                                                    |    n/a     |   ✅   |   ✅    |  ✅  |    ✅    |        ✅        |                                    |        ✅         | Manual       |
| [FFTW](http://www.fftw.org/)                                                            |   3.3.10   |   ✅   |   ✅    |  ✅  |    ✅    |        ✅        |                                    |        ✅         | Vcpkg        |
| [Intel MKL](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onemkl.html) |    n/a     |   ✅   |   ✅    |  ✅  |    ✅    |        ✅        |                                    |        ✅         | Manual       |
| [Intel IPP](https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp.html)    |    n/a     |   ✅   |   ✅    |  ✅  |    ❌    |        ✅        |                                    |        ✅         | Manual       |
| [Sleef](https://sleef.org/)                                                             |   3.9.0    |   ✅   |   ✅    |  ✅  |    ❌    |        ❌        | Power-of-2 sizes only              |        ✅         | Vcpkg        |
| [PFFFT](https://bitbucket.org/jpommier/pffft/)                                          |   1.0.0    |   ✅   |   ❌    |  ✅  |    ❌    |        ❌        | multiples of SIMD size²            |        ❌         | Vcpkg        |
| [JUCE](https://juce.com/)                                                               |   8.0.7    |   ✅   |   ❌    |  ✅  |    ❌    |        ❌        |                                    |        ✅         | Vcpkg        |
| [KissFFT](https://github.com/mborgerding/kissfft)                                       |  131.2.0   |   ✅   |   ✅    |  ✅  |    ❌    |        ✅        | No real inverse; out-of-place only |        ❌         | Bundled      |
| [OTFFT](https://github.com/DEWETRON/otfft)                                              |   11.5.0   |   ❌   |   ✅    |  ✅  |    ❌    |        ✅        | x86 only, real requires even sizes |        ❌         | Fetch        |
| [PocketFFT](https://github.com/mreineck/pocketfft)                                      | 2023-09-25 |   ✅   |   ✅    |  ✅  |    ❌    |        ✅        |                                    |        ❌         | Vcpkg        |
| [vDSP (Accelerate)](https://developer.apple.com/documentation/accelerate/vdsp)          |    n/a     |   ✅   |   ✅    |  ✅  |    ❌    |        ✅        | Apple only                         |       n/a        | System       |
| [libsamplerate](http://www.mega-nerd.com/SRC/)                                          |   0.2.2    |   ✅   |   ❌    | n/a |   n/a   |       n/a       | SRC only                           |        ❌         | Vcpkg        |
| [soxr](https://sourceforge.net/projects/soxr/)                                          |   0.1.3    |   ✅   |   ✅    | n/a |   n/a   |       n/a       | SRC only                           |        ❌         | Vcpkg        |
| [speexdsp](https://github.com/xiph/speexdsp)                                            |   1.2.1    |   ✅   |   ❌    | n/a |   n/a   |       n/a       | SRC only                           |        ❌         | Vcpkg        |
| [r8brain-free-src](https://github.com/avaneev/r8brain-free-src)                         |    7.1     |   ✅   |   ✅    | n/a |   n/a   |       n/a       | SRC only                           |        ❌         | Fetch        |

> [!NOTE]
> Not every library included here was designed with maximum performance as its primary goal. The selection is based on popularity and widespread use in real-world projects, rather than expected raw performance — so please keep that in mind when interpreting the results.

## Building

### Requirements

* C++17 compiler (Clang 12.0+ recommended)
* CMake 3.12 or newer
* Python 3.5 or newer (for plotting)
  * matplotlib
  * numpy

### Setup

The project supports two build configurations controlled by the `OPEN` option:

- **`OPEN=ON`** (default) — compiles only open-source libraries (KFR, FFTW, PFFFT, JUCE, KissFFT, Sleef, libsamplerate, soxr, speexdsp, r8brain, PocketFFT, OTFFT). The resulting package is GPLv3-compatible.
- **`OPEN=OFF`** — compiles only closed-source/proprietary libraries (Intel IPP, Intel MKL, and vDSP on Apple). Open-source libraries are excluded.

See [`.github/workflows/build-open.yml`](.github/workflows/build-open.yml) and [`.github/workflows/build-proprietary.yml`](.github/workflows/build-proprietary.yml) for CI examples of each configuration.

To compile only a single library, pass `-DSINGLE=<libname>` (e.g. `-DSINGLE=kfr`). The resulting package will have the same license as that library.

Some libraries are available as packages in vcpkg and they will be found automatically on cmake configuration as vcpkg is bundled as submodule. Some libraries (e.g. KFR, Intel libraries) require manual installation, so you need to specify the paths to their CMake configs in `CMAKE_PREFIX_PATH`.

> [!WARNING]
> Each library has its own license. When multiple libraries are combined, the most restrictive license applies — **GPL always wins**.

Example:
```
C:/Program Files (x86)/Intel/oneAPI/ipp/2021.9.0/lib/cmake/ipp
C:/Program Files (x86)/Intel/oneAPI/mkl/2026.0/lib/cmake/mkl
kfr-install-dir/lib/cmake
```

### Build

```bash
cmake -B build -DCMAKE_PREFIX_PATH="path1;path2;..."
cmake --build build
```

A separate executable is produced for each library found (e.g. `fft_benchmark_kfr`, `fft_benchmark_ipp`, etc.).

> [!NOTE]
> All libraries are **optional** — if not found via CMake's `find_package`, they are simply disabled.

> [!NOTE]
> Linux prebuilt binaries published on GitHub Actions are built against glibc 2.34 (Ubuntu 22.04+, Debian 12+).

> [!NOTE]
> Windows builds require the [Visual Studio 2022 x64 redistributable packages](https://aka.ms/vs/17/release/vc_redist.x64.exe).

## Usage

```
fft_benchmark_<library> [options] <size> [<size> ...]
```

Example:
```bash
fft_benchmark_kfr --save results.json 262144 512x512 64x64x64
fft_benchmark_kfr --save - 262144   # print JSON to stdout
```

## Options

> [!NOTE]
> Multithreading is disabled for fair comparison, as only a few libraries support it.


| Option             | Description                                                     |
|--------------------|-----------------------------------------------------------------|
| `SIZE`             | 1D FFT                                                          |
| `SIZExSIZE`        | 2D FFT. Example: `64x32`                                        |
| `SIZExSIZExSIZE`   | 3D FFT. Example: `64x32x16`                                     |
| `--complex flags`  | `y` (complex tests), `yn` (all tests), `n` (real tests)         |
| `--inverse flags`  | `y` (IDFT tests), `ny` (DFT/IDFT tests), `n` (DFT tests)        |
| `--inplace flags`  | `y` (inplace tests), `ny` (all tests), `n` (out-of-place tests) |
| `--save data.json` | Save results in JSON                                            |
| `--save -`         | Print resulting JSON to stdout                                  |
| `--avx2-only`      | Enable only AVX2 (supported in KFR, IPP, MKL)                   |
| `--no-progress`    | Disable verbose progress output                                 |
| `--no-banner`      | Disable banner                                                  |

## Plotting results

Use `plot.py` to generate comparison charts from the JSON output of multiple benchmark runs:

```bash
python plot.py results_kfr.json results_ipp.json results_fftw.json
```

This generates SVG plots for every combination of data type, transform type, direction, and buffer mode (e.g. `float-complex-forward-inplace.svg`).

## JSON output format

Each benchmark run produces a JSON file with the following structure:

```json
{
    "cpu": "...",
    "clock_MHz": 3600.0,
    "library": "...",
    "results": [
        {
            "size": 1024,
            "data": "float",
            "type": "complex",
            "direction": "forward",
            "buffer": "outofplace",
            "mflops": 12345.67,
            "best_time": 0.83,
            "median_time": 0.91
        }
    ]
}
```

For multidimensional transforms, `size` is an array (e.g. `[512, 512]`).

## Legal Disclaimer

All trademarks, product names, and company names are the property of their respective owners and are used for identification purposes only.

## Author

Dan Casarín, the author of [KFR](https://kfr.dev)

## License

MIT

