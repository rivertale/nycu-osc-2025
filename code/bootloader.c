#include "common.h"
#include "uart.h"
#include "bootloader_boot.h"
#include "bootloader.h"

#include "uart.c"

static void
print_string(c8 *string)
{
    umm len = string_len(string);
    mini_uart_write(string, len);
}

static b32
load_kernel(void *addr)
{
    b32 success = 0;
    
    u32 magic = 0;
    mini_uart_read(&magic, sizeof(magic));
    if(magic == BOOT_MAGIC)
    {
        BootHeader header = {0};
        header.magic = magic;
        mini_uart_read(&header.size, sizeof(header.size));
        mini_uart_read(&header.checksum, sizeof(header.checksum));
        mini_uart_read(addr, header.size);
        u32 checksum = calculate_kernel_checksum(addr, header.size);
        success = (checksum == header.checksum);
    }
    return success;
}

void
bootloader_main(void *devicetree_addr)
{
    mini_uart_init();
    for(;;)
    {
        mini_uart_write_const(BOOTLOAD_WAIT_KERNEL_MESSAGE);
        if(load_kernel(KERNEL_STARTUP_ADDR))
        {
            __asm__ volatile("mov x0, %0\n"
                             "ldr x1, =%1\n"
                             "br x1\n"
                             :: "r"(devicetree_addr), "i"(KERNEL_STARTUP_ADDR));
        }
    }
}
