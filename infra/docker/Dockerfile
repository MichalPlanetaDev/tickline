FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        libsqlite3-dev \
        make \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --parallel
RUN ctest --test-dir build --output-on-failure
RUN PYTHONPATH=tools/python python3 -m unittest discover -s tools/python/tests -v
RUN bash scripts/check-docs.sh
