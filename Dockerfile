FROM debian:11
#FROM moddevices/mod-plugin-builder:latest
#docker pull cbix/mod-plugin-builder:moddwarf

RUN apt update \
 && apt install --no-install-recommends --yes \
    git make gcc build-essential lv2-c++-tools \
    ca-certificates python3 python-is-python3 python3-pip \
    python3-setuptools python3-wheel ninja-build \
    git \
 && apt-get clean

RUN pip3 install meson

RUN cd /tmp \
 && git clone https://github.com/lv2/lv2.git \
 && cd lv2 \
 && git checkout v1.18.10 \
 && cd /tmp/lv2 && meson setup build && meson compile -C build && meson install -C build \
 && rm -r /tmp/lv2

COPY src/ /app
WORKDIR /app

RUN make build
