FROM ubuntu:20.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends git make python2 ca-certificates wget \
 && apt-get clean -y \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
RUN wget -qO- https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v8.3.1-1.4/xpack-arm-none-eabi-gcc-8.3.1-1.4-linux-x64.tar.gz | tar -zxv
ENV PATH $PATH:/app/xpack-arm-none-eabi-gcc-8.3.1-1.4/bin
