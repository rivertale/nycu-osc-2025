#!/bin/sh

cd ../build
lldb -f kernel8.elf -o "gdb-remote 1234"
