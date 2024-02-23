# Use the rootproject/root image as the base image
FROM rootproject/root:latest

# Update package lists and install necessary packages
ENV LANG=C.UTF-8
ENV DEBIAN_FRONTEND noninteractive
ENV NPROC=8

COPY Dockerfile.packages packages

RUN apt-get update -qq && \
    ln -sf /usr/share/zoneinfo/UTC /etc/localtime && \
    apt-get -y install $(cat packages | grep -v '#') && \
    apt-get autoremove -y && \
    apt-get clean -y && \
    rm -rf /var/cache/apt/archives/* && \
    rm -rf /var/lib/apt/lists/*

ENV LD_LIBRARY_PATH /usr/local/lib:/usr:/usr/lib/aarch64-linux-gnu:/usr/lib/x86_64-linux-gnu

COPY . /opt/fft-benchmark
WORKDIR /opt/fft-benchmark

# KFR5/6
RUN wget https://github.com/kfrlib/kfr/archive/refs/tags/6.0.2.tar.gz
RUN tar -xzvf 6.0.2.tar.gz
RUN cd kfr-6.0.2 && mkdir build
RUN cd kfr-6.0.2/build && cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/opt/kfr/6.0.2
RUN cd kfr-6.0.2/build && make -j8 && make install

RUN wget https://github.com/kfrlib/kfr/archive/refs/tags/5.2.0.tar.gz
RUN tar -xzvf 5.2.0.tar.gz
RUN cd kfr-5.2.0 && mkdir build
RUN cd kfr-5.2.0/build && cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/opt/kfr/5.2.0
RUN cd kfr-5.2.0/build && make -j8 && make install

RUN wget https://github.com/kfrlib/kfr/archive/refs/tags/5.0.0.tar.gz
RUN tar -xzvf 5.0.0.tar.gz
RUN cd kfr-5.0.0 && mkdir build
RUN cd kfr-5.0.0/build && cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/opt/kfr/5.0.0
RUN cd kfr-5.0.0/build && make -j8 && make install

# FFTW3 (Optional)
RUN apt-get update && apt-get install libfftw3-dev || true

# Intel IPP (Optional)
RUN wget https://registrationcenter-download.intel.com/akdlm/IRC_NAS/046b1402-c5b8-4753-9500-33ffb665123f/l_ipp_oneapi_p_2021.10.1.16.sh || true
RUN ./l_ipp_oneapi_p_2021.10.1.16.sh || true

# Intel MKP (Optional)
RUN apt-get update && apt-get install intel-oneapi-mkl-devel || true

CMD ["make", "all"]
