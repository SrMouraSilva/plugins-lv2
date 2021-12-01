FROM debian:11
#FROM moddevices/mod-plugin-builder:latest

RUN apt update \
 && apt install --no-install-recommends --yes \
    git make gcc build-essential lv2-c++-tools \
    ca-certificates python2 python-is-python2 \
    git \
 && apt-get clean

RUN cd /tmp \
 && git clone https://github.com/lv2/lv2.git --recursive \
 && cd lv2 \
 && git checkout v1.18.2 \
 && git fetch --recurse-submodules -j2 \
 && ./waf configure --docs \
 && ./waf \
 && ./waf install \
 && rm -r /tmp/lv2

COPY . /app
WORKDIR /app

RUN make build
