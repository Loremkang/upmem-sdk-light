#!/bin/bash

g++ \
    -Wall -Werror -std=c++17 \
    -g -O0 -march=native \
    -I ../pim_interface \
    -I ../third_party/upmem-sdk/src \
    -I ../third_party/upmem-sdk/src/backends/api/include/api \
    -I ../third_party/upmem-sdk/src/backends/api/include/lowlevel \
    -I ../third_party/upmem-sdk/src/backends/api/src/include \
    -I ../third_party/upmem-sdk/src/backends/commons/include \
    -I ../third_party/upmem-sdk/src/backends/commons/src/properties \
    -I ../third_party/upmem-sdk/src/backends/hw/src/rank \
    -I ../third_party/upmem-sdk/src/backends/hw/src/commons \
    -I ../third_party/upmem-sdk/src/backends/ufi/include/ufi \
    -I ../third_party/upmem-sdk/src/backends/ufi/include \
    -I ../third_party/upmem-sdk/src/backends/verbose/src \
    `dpu-pkg-config --cflags --libs dpu` \
    host.cpp -o host

dpu-upmem-dpurte-clang \
    -g -O2 \
    dpu.c -o dpu
