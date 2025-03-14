#!/bin/sh

set -e
mkdir -p ../build
cd ../build

clang -g -mcpu=cortex-a53 --target=aarch64-rpi3-elf -c ../code/startup.S ../code/main.c
ld.lld -m aarch64elf -T ../code/linker.ld -o kernel8.elf startup.o main.o
llvm-objcopy --output-target=aarch64-rpi3-elf -O binary kernel8.elf kernel8.img
rm *.o
