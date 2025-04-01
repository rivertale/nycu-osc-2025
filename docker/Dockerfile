FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y clang cpio lld lldb llvm qemu-system-aarch64 screen sudo && \
    userdel -r ubuntu
