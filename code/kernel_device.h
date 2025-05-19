#ifndef KERNEL_DEVICE_H
#define KERNEL_DEVICE_H


// devicetree
#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

#define DEVICETREE_MAX_DIR_LEN 256
#define devicetree_u32(ptr) ((u32)((((u8 *)ptr)[0] << 24) | (((u8 *)ptr)[1] << 16) | (((u8 *)ptr)[2] << 8) | (((u8 *)ptr)[3])))

// cpio
#define CPIO_MAGIC "070701"
#define CPIO_SENTINEL_FILE "TRAILER!!!"

typedef struct FdtHeader
{
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
} FdtHeader;

typedef struct  __attribute__((aligned(8))) ArmMemoryInfo
{
    u32 base;
    u32 size;
} ArmMemoryInfo;

typedef struct DevicetreeIter
{
    u8 *cur;
    c8 *prop;
    u8 *data;
    u32 size;
    s32 depth;

    u32 dir_len;
    c8 *strings_block;
    c8 dir[DEVICETREE_MAX_DIR_LEN];
} DevicetreeIter;

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

#endif //KERNEL_DEVICE_H
