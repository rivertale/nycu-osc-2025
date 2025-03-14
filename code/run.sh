#!/bin/sh

cd ../build
qemu-system-aarch64 -M raspi3b -kernel bootloader.img -initrd initramfs.cpio -display none -dtb ../data/sdcard/bcm2710-rpi-3-b-plus.dtb -serial null -serial pty -S -s
