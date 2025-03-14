#ifndef CPIO_H
#define CPIO_H

#define CPIO_MAGIC "070701"
#define CPIO_SENTINEL_FILE "TRAILER!!!"

typedef struct CpioNewcHeader
{
    c8 magic[6]; // CPIO_MAGIC
    c8 inode[8];
    c8 mode[8];
    c8 uid[8];
    c8 gid[8];
    c8 nlink[8];
    c8 mtime[8];
    c8 filesize[8];
    c8 devmajor[8];
    c8 devminor[8];
    c8 rdevmajor[8];
    c8 rdevminor[8];
    c8 namesize[8]; // including null-terminator
    c8 check[8];
    
    // c8[] null-terminated filename
    // pad to 2-byte boundary
    // u8[] file content
    // pad to 4-byte boundary
} CpioNewcHeader;

static void *g_cpio_base = 0;

#endif //CPIO_H
