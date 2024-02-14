# Use the rootproject/root image as the base image
FROM debian:stable-slim

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

ENV LD_LIBRARY_PATH /usr/local/lib:/usr:/usr/lib/aarch64-linux-gnu

WORKDIR /opt

RUN wget https://github.com/kfrlib/kfr/archive/refs/tags/6.0.2.tar.gz
RUN tar -xzvf 6.0.2.tar.gz
RUN cd kfr-6.0.2 && mkdir build
RUN cd kfr-6.0.2/build && cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON
RUN cd kfr-6.0.2/build && make -j8 && make install

RUN git clone https://github.com/kfrlib/fft-benchmark
RUN cd fft-benchmark && mkdir -p build
RUN cd fft-benchmark/build && cmake ..  -DCMAKE_PREFIX_PATH="/usr/local/lib/cmake"
RUN cd fft-benchmark/build && make -j8

CMD ["/bin/bash"]
