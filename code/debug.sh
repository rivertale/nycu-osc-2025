#!/bin/sh

cd ../build
lldb -f bootloader.elf -o "target modules add kernel.elf" -o "gdb-remote 1234"
