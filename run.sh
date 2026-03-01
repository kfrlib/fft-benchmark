#!/bin/bash

SIZES='64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576'

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

    echo "================================"
    echo "Running benchmark for ${LIB}..."
    
    build/fft_benchmark_${LIB}${SUFFIX} --save data/results_${LIB}.json --complex y --inverse n --no-banner ${SIZES}
done
