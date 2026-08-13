FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        git \
        libsdl2-dev \
        make \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
