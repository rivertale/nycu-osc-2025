#!/bin/sh

set -e

mnt_dir="/mnt/usb"

if [ $(id -u) -ne 0 ]; then
    echo "ERROR: The script must run under sudo"
    exit
fi

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 device_name partition_name image_file"
    echo "  device_name       Name of the device (For example: sda, sdb, ...)"
    echo "  partition_name    Name of the partition (For example: sda1, sdb1, ...)"
    echo "  image_file        Path of the image file (For example: bootloader.img)"
    echo ""
    echo "You might be looking for the device:"
    lsblk -f | grep "sd"
else
    if mount | grep -qs "/dev/$2"; then
        umount "/dev/$2"
    fi
    mkfs.fat -F 32 "/dev/$2" >/dev/null
    mkdir -p ${mnt_dir}
    mount "/dev/$2" "${mnt_dir}"
    cp "$3" "${mnt_dir}/kernel8.img"
    echo "Mounted to ${mnt_dir}"
fi
