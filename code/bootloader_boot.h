#ifndef BOOTLOADER_BOOT_H
#define BOOTLOADER_BOOT_H

#define BOOTLOAD_WAIT_KERNEL_MESSAGE "Waiting for kernel...\r\n"
#define BOOT_MAGIC (('B' << 0) | ('O' << 8) | ('O' << 16) | ('T' << 24))

typedef struct BootHeader
{
    u32 magic; // BOOT_MAGIC
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
        case 3: { tail |= (buffer[2] << 16); } break;
        case 2: { tail |= (buffer[1] << 8); } break;
        case 1: { tail |= (buffer[0] << 0); } break;
        default: { do_nothing; } break;
    }
    checksum ^= tail;
    return checksum;
}

#endif //BOOTLOADER_BOOT_H
