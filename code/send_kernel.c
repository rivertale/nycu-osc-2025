#include "common.h"
#include "bootloader_boot.h"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

typedef struct LoadedFile
{
    u32 size;
    u8 *buffer;
} LoadedFile;

static void
fatal_error(c8 *message)
{
    printf("%s\n", message);
    printf("errno=%d\n", errno);
    exit(1);
}

static int
open_serial_device(c8 *path)
{
    int file = open(path, O_RDWR | O_NOCTTY);
    
    if(file == -1)
        fatal_error("open() failed");
    
    if(!isatty(file))
        fatal_error("is not a tty");
    
    struct termios attr;
    if(tcgetattr(file, &attr) != 0)
        fatal_error("tcgetattr() failed");
    
    attr.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    attr.c_oflag &= ~(OPOST | OLCUC | ONLCR | OCRNL);
    attr.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOCTL | ECHOPRT | ECHOKE | IEXTEN);
    attr.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
    attr.c_cflag |= CS8 | CREAD | CLOCAL;
    attr.c_cc[VMIN] = 1;
    attr.c_cc[VTIME] = 0;
    
    if(cfsetspeed(&attr, B115200) != 0)
        fatal_error("cfsetspeed() failed");
    
    if(tcflush(file, TCIFLUSH) != 0)
        fatal_error("tcflush() failed");
    
    if(tcsetattr(file, TCSANOW, &attr) != 0)
        fatal_error("tcsetattr() failed");
    
    return file;
}

static void
read_serial_device(int file, void *buffer, umm size)
{
    u8 *cur = (u8 *)buffer;
    while(size > 0)
    {
        ssize_t byte_read = read(file, cur, size);
        if(byte_read == -1)
            fatal_error("read() failed");
        
        cur += byte_read;
        size -= byte_read;
    }
}

static void
write_serial_device(int file, void *buffer, umm size)
{
    u8 *cur = (u8 *)buffer;
    while(size > 0)
    {
        ssize_t byte_written = write(file, cur, 1);
        if(byte_written == -1)
            fatal_error("write() failed");
        
        usleep(300);
        cur += byte_written;
        size -= byte_written;
    }
}

static LoadedFile
read_entire_file(c8 *path)
{
    LoadedFile result = {0};
    
    int file = open(path, O_RDONLY);
    if(file == -1)
        fatal_error("open() failed");
    
    result.size = lseek(file, 0, SEEK_END);
    if(result.size == -1)
        fatal_error("lseek(SEEK_END) failed");
    
    if(lseek(file, 0, SEEK_SET) == -1)
        fatal_error("lseek(SEEK_SET) failed");
    
    result.buffer = (u8 *)malloc(result.size);
    if(!result.buffer)
        fatal_error("out of memory");
    
    if(read(file, result.buffer, result.size) != result.size)
        fatal_error("read() failed");
    
    if(close(file) == -1)
        fatal_error("close() failed");
    
    return result;
}

int
main(int arg_count, c8 **args)
{
    if(arg_count != 3)
    {
        printf("usage: send_kernel <kernel_path> <tty_num>\n");
        return 0;
    }
    
    c8 *kernel_path = args[1];
    c8 device_path[256];
    if(args[2][0] == 'p')
        snprintf(device_path, sizeof(device_path), "/dev/pts/%s", args[2] + 1);
    else
        snprintf(device_path, sizeof(device_path), "/dev/ttyUSB%s", args[2]);
    
    LoadedFile kernel = read_entire_file(kernel_path);
    BootHeader header = {0};
    header.magic = BOOT_MAGIC;
    header.size = kernel.size;
    header.checksum = calculate_kernel_checksum(kernel.buffer, kernel.size);
    
    int device_file = open_serial_device(device_path);
    write_serial_device(device_file, &header, sizeof(header));
    write_serial_device(device_file, kernel.buffer, kernel.size);
    
    // NOTE: we must sleep before closing for it to work, why? 
    sleep(1);
    
    if(close(device_file) != 0)
        fatal_error("close() failed");
    
    free(kernel.buffer);
    return 0;
}