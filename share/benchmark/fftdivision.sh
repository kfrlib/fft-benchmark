#!/bin/bash

if [ $# -ne 3 ]; then
    echo "Usage: $0 FFTOUTPUT FFTALGOS FFTSIZE_MAX FFTDIV"
    exit 1
fi

FFTOUTPUT="$1"
FFTALGOS="$2"
FFTSIZE="$2"
FFTDIV="$3"

mkdir -p "$FFTOUTPUT"
for FFTALGO in $FFTALGOS; do

    if [ ! -f "$(which fft_benchmark_${FFTALGO})"]; then  
        echo "Benchmark test for $FFTALGO not found. skip."
    fi

    for i in {1..$FFTDIV}; do

        FFTSIZE=$((FFTSIZE_MAX / NDIV * i))
        if [ ! -f "$FFTOUTPUT/${FFTALGO}-${FFTSIZE}.txt" ]; then
            echo -ne "Running benchmark for $FFTALGO(N = $FFTSIZE): "
            fft_benchmark_${FFTALGO} $FFTSIZE --save $FFTOUTPUT/${FFTALGO}-$FFTSIZE.json
            echo "results stored in $FFTOUTPUT/${FFTALGO}-$FFTSIZE.json"
        else
            echo "File for for $FFTALGO(N = $FFTSIZE) already exists."
        fi

    done
done