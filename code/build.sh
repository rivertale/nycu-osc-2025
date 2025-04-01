#!/bin/sh

set -e
mkdir -p ../build
cd ../build

code_dir="$(realpath ../code)"
build_dir="$(realpath ../build)"
data_dir="$(realpath ../data)"

compile_flags="-g -mcpu=cortex-a53 --target=aarch64-rpi3-elf -c"
link_flags="-m aarch64elf"

echo "Compiling bootloader..."
clang-18 ${compile_flags} ${code_dir}/bootloader_startup.S ${code_dir}/bootloader.c
ld.lld-18 ${link_flags} -T ${code_dir}/bootloader.ld -o bootloader.elf bootloader_startup.o bootloader.o
llvm-objcopy-18 --output-target=aarch64-rpi3-elf -O binary bootloader.elf bootloader.img

echo "Compiling kernel..."
clang-18 ${compile_flags} ${code_dir}/kernel_startup.S ${code_dir}/kernel.c
ld.lld-18 ${link_flags} -T ${code_dir}/kernel.ld -o kernel.elf kernel_startup.o kernel.o
llvm-objcopy-18 --output-target=aarch64-rpi3-elf -O binary kernel.elf kernel.img

echo "Compiling tools"...
clang-18 -g -o send_kernel ${code_dir}/send_kernel.c
clang-18 -g -o cal ${code_dir}/cal.c

echo "Building initial file system..."
cd ${data_dir}/initramfs
find . | cpio -o -H newc > ${build_dir}/initramfs.cpio 2>/dev/null
cd ${build_dir}

rm *.o
