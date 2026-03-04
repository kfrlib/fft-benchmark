# FFT benchmark

A benchmark for comparison of FFT algorithms performance.
Supports 1D, 2D, and 3D transforms with float and double precision.
Measures the performance of real/complex, in-place/out-of-place, forward/inverse FFT.

## Supported libraries

| Library                                                                                 | Float | Double | 1D | 2D & 3D | Notes                              | Installation |
|-----------------------------------------------------------------------------------------|:-----:|:------:|:--:|:-------:|------------------------------------|--------------|
| [KFR](https://github.com/kfrlib/kfr)                                                    |   ✅   |   ✅    | ✅  |    ✅    | Real transforms require even sizes | Manual       |
| [Intel IPP](https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp.html)    |   ✅   |   ✅    | ✅  |    ❌    |                                    | Manual       |
| [Intel MKL](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onemkl.html) |   ✅   |   ✅    | ✅  |    ✅    |                                    | Manual       |
| [FFTW](http://www.fftw.org/)                                                            |   ✅   |   ✅    | ✅  |    ✅    |                                    | Vcpkg        |
| [Sleef](https://sleef.org/)                                                             |   ✅   |   ✅    | ✅  |    ❌    |                                    | Vcpkg        |
| [PFFFT](https://bitbucket.org/jpommier/pffft/)                                          |   ✅   |   ❌    | ✅  |    ❌    | Float-only                         | Vcpkg        |
| [JUCE](https://juce.com/)                                                               |   ✅   |   ❌    | ✅  |    ❌    | Float-only, power-of-2 sizes only  | Vcpkg        |
| [KissFFT](https://github.com/mborgerding/kissfft)                                       |   ✅   |   ✅    | ✅  |    ❌    | No real inverse; out-of-place only | Bundled      |

**Note:** Multithreading is disabled for fair comparison, as only a few libraries support it.

All libraries are **optional** — if not found via CMake's `find_package`, they are simply disabled.

## Building

### Requirements

* C++17 compiler (Clang 12.0+ recommended)
* CMake 3.12 or newer
* Python 3.5 or newer (for plotting)
  * matplotlib
  * numpy

### Setup

See [`.github/workflows/build.yml`](.github/workflows/build.yml) for an example of how to set up the environment for building and running the benchmarks.

Some libraries are available as packages in vcpkg and they will be found automatically on cmake configuration as vcpkg is bundled as submodule. Some libraries (e.g. KFR, Intel libraries) require manual installation, so you need to specify the paths to their CMake configs in `CMAKE_PREFIX_PATH`.

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

