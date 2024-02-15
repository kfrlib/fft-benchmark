#! /bin/bash

# KFR 5
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PWD/../install \
         -DKFR5="$KFR5" -DKFR6="$KFR6" \
         -DFFTW="$FFTW" \
         -DIPP="$IPP" \
         -DMKL="$MKL"\

make -j8 && make install
cd ..
