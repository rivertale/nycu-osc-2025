#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#define KERNEL_SPACE_OFFSET 0x0000000000000000ull

#include "common.h"
#include "intrinsic.h"
#include "peripheral.h"

#define BOOTLOADER_STARTUP_ADDR ((void *)0x40000)
#define KERNEL_STARTUP_ADDR ((void *)0x80000)
#define KERNEL_MAGIC (('B' << 0) | ('O' << 8) | ('O' << 16) | ('T' << 24))

typedef struct BootHeader
{
    u32 magic; // KERNEL_MAGIC
    u32 size;
    u32 checksum;
} BootHeader;

static u32
calculate_kernel_checksum(u8 *buffer, um32 size)
{
    u32 checksum = 0;
    while(size >= 4)
    {
        checksum ^= *(u32 *)buffer;
        buffer += 4;
        size -= 4;
    }

    u32 tail = 0;
    switch(size)
    {
        case 3: { tail |= (buffer[2] << 16); }
        case 2: { tail |= (buffer[1] << 8); }
        case 1: { tail |= (buffer[0] << 0); } break;
        default: { do_nothing; } break;
    }
    checksum ^= tail;
    return checksum;
}

#endif //BOOTLOADER_H
