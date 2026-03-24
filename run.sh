#!/bin/bash

SIZES='32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216'

# .exe on windows
SUFFIX=""
if [[ "$OSTYPE" == "msys" ]]; then
    SUFFIX=".exe"
fi

mkdir -p data

for LIB in kfr fftw ipp mkl sleef kissfft pffft juce vdsp; do
    if [ ! -f "build/fft_benchmark_${LIB}${SUFFIX}" ]; then
        echo "Warning: build/fft_benchmark_${LIB}${SUFFIX} not found. Skipping ${LIB} benchmark."
        continue
    fi

    # Skip if json file already exists
    if [ -f "data/results_${LIB}.json" ]; then
        echo "Results for ${LIB} already exist. Skipping benchmark."
        echo "To rerun the benchmark for ${LIB}, delete data/results_${LIB}.json and run this script again."
        continue
    fi

    echo "================================"
    echo "Running benchmark for ${LIB}..."
    
    build/fft_benchmark_${LIB}${SUFFIX} --save data/results_${LIB}.json --complex y --inverse n --no-banner ${SIZES}
done
