#!/bin/bash

# Parse --avx512 flag (must be first argument)
USE_AVX512=false
if [ "$1" = "--avx512" ]; then
    USE_AVX512=true
    shift
fi

# Parse --runs N flag (must follow --avx512 if present)
RUNS=1
if [ "$1" = "--runs" ]; then
    shift
    RUNS="$1"
    shift
    if ! [[ "$RUNS" =~ ^[0-9]+$ ]] || [ "$RUNS" -lt 1 ]; then
        echo "Error: --runs requires a positive integer"
        exit 1
    fi
fi

if [ $# -eq 0 ]; then
    echo "Usage: run.sh [--avx512] [--runs N] <benchmark-args...>"
    echo ""
    echo "  --avx512      Run avx512 builds instead of avx2 builds (must be first argument)"
    echo "  --runs N      Repeat all benchmarks N times, saving to results_<tag>_<run>.json"
    echo ""
    echo "All remaining arguments are forwarded to the benchmark executable."
    exit 1
fi

# .exe on windows
SUFFIX=""
if [[ "$OSTYPE" == "msys" ]]; then
    SUFFIX=".exe"
fi

mkdir -p data

FOUND=false

for RUN in $(seq 1 "$RUNS"); do
    if [ "$RUNS" -gt 1 ]; then
        echo "################################"
        echo "Run ${RUN}/${RUNS}"
        echo "################################"
    fi

    for BENCH in build/fft_benchmark_*${SUFFIX}; do
        if [ ! -f "$BENCH" ]; then
            continue
        fi

        # On *nix, skip if not executable
        if [[ "$OSTYPE" != "msys" ]] && [ ! -x "$BENCH" ]; then
            echo "Warning: $BENCH is not executable. Skipping."
            continue
        fi

        # Extract the library+arch tag (e.g. "kfr-any" or "kissfft-avx2")
        BENCHNAME=$(basename "$BENCH")
        LIBTAG="${BENCHNAME#fft_benchmark_}"
        # Strip .exe suffix on Windows
        LIBTAG="${LIBTAG%.exe}"

        # Skip based on --avx512 flag
        if $USE_AVX512; then
            # Only run -any or -avx512, skip -avx2
            if [[ "$LIBTAG" == *-avx2 ]]; then
                echo "Skipping ${LIBTAG} (--avx512 set, ignoring avx2 builds)"
                continue
            fi
        else
            # Only run -any or -avx2, skip -avx512
            if [[ "$LIBTAG" == *-avx512* ]]; then
                echo "Skipping ${LIBTAG} (--avx512 not set, ignoring avx512 builds)"
                continue
            fi
        fi

        OUTFILE="data/results_${LIBTAG}_${RUN}.json"

        # Skip if json file already exists
        if [ -f "$OUTFILE" ]; then
            echo "Results for ${LIBTAG} run ${RUN} already exist. Skipping benchmark."
            echo "To rerun, delete ${OUTFILE} and run this script again."
            continue
        fi

        echo "================================"
        echo "Running benchmark for ${LIBTAG} (run ${RUN}/${RUNS})..."
        
        "$BENCH" --save "$OUTFILE" --no-banner "$@"
        FOUND=true
    done
done

if ! $FOUND; then
    echo "Warning: No benchmark executables found in build/"
fi
