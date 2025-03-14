#!/bin/sh

set -e

mnt_dir="/mnt/usb"

if [ $(id -u) -ne 0 ]; then
    echo "ERROR: The script must run under sudo"
    exit
fi

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 partition_name"
    echo "  partition_name    Name of the partition (For example: sda1, sdb1, ...)"
    echo ""
    echo "You might be looking for the device:"
    lsblk -f | grep "sd"
else
    if mount | grep -qs "/dev/$1"; then
        umount "/dev/$1"
    fi
    mount "/dev/$1" "${mnt_dir}"
    cp ../build/bootloader.img "${mnt_dir}/kernel8.img"
    cp ../build/initramfs.cpio "${mnt_dir}"
    cp ../data/sdcard/* "${mnt_dir}"
    umount /mnt/usb
fi
