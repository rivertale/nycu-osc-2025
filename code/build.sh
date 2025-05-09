#!/bin/sh

set -e
mkdir -p ../build
cd ../build

code_dir="$(realpath ../code)"
build_dir="$(realpath ../build)"
data_dir="$(realpath ../data)"

compile_flags="-g -mcpu=cortex-a53 --target=aarch64-rpi3-elf -c"
link_flags="-m aarch64elf"
image_flags="--output-target=aarch64-rpi3-elf -O binary"

echo "Compiling bootloader..."
clang-18 ${compile_flags} ${code_dir}/bootloader.S -o bootloader_asm.o
clang-18 ${compile_flags} ${code_dir}/bootloader.c -o bootloader.o
ld.lld-18 ${link_flags} -T ${code_dir}/bootloader.ld bootloader_asm.o bootloader.o -o bootloader.elf
llvm-objcopy-18 ${image_flags} bootloader.elf bootloader.img

echo "Compiling kernel..."
clang-18 ${compile_flags} ${code_dir}/kernel.S -o kernel_asm.o
clang-18 ${compile_flags} ${code_dir}/kernel.c -o kernel.o
ld.lld-18 ${link_flags} -T ${code_dir}/kernel.ld kernel_asm.o kernel.o -o kernel.elf
llvm-objcopy-18 ${image_flags} kernel.elf kernel.img

echo "Compiling user programs..."
clang-18 ${compile_flags} ${code_dir}/user_exception.S
ld.lld-18 ${link_flags} user_exception.o -o user_exception.elf
llvm-objcopy-18 ${image_flags} user_exception.elf user_exception.img

echo "Compiling tools..."
clang-18 -g -o send_kernel ${code_dir}/send_kernel.c

echo "Building initial file system..."
cd ${data_dir}/initramfs
cp ${build_dir}/user_exception.img ./
# find . | cpio -o -H newc > ${build_dir}/initramfs.cpio 2> /dev/null
cd ${build_dir}

rm *.o
