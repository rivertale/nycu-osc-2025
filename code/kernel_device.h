#ifndef KERNEL_DEVICE_H
#define KERNEL_DEVICE_H

// watchdog
#define PM_RSTC 0x3F10001c
#define PM_WDOG 0x3F100024

#define PM_PASSWORD 0x5a000000
#define PM_RSTC_WRCFG_FULL_RESET 0x20

// mailbox
#define MAILBOX_READ 0x3f00b880
#define MAILBOX_STATUS 0x3f00b898
#define MAILBOX_WRITE 0x3f00b8a0

#define MAILBOX_EMPTY 0x40000000
#define MAILBOX_FULL 0x80000000

#define MAILBOX_REQUEST 0x00000000
#define MAILBOX_RESPONSE_SUCCESS 0x80000000
#define MAILBOX_RESPONSE_ERROR 0x80000001

#define MAILBOX_TAG_END 0x00000000
#define MAILBOX_TAG_REQUEST 0x00000000
#define MAILBOX_TAG_GET_BOARD_REVISION 0x00010002
#define MAILBOX_TAG_GET_ARM_MEMORY 0x00010005

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

typedef struct ArmMemoryInfo
{
    u32 base;
    u32 size;
} __attribute__((aligned(8))) ArmMemoryInfo;

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

// TODO: it would be better if we combine it with MemoryRegionList
typedef struct DeviceRegionList
{
    void *devicetree_begin;
    void *devicetree_end;
    void *spin_table_begin;
    void *spin_table_end;
    void *cpio_begin;
    void *cpio_end;
} DeviceRegionList;

#endif //KERNEL_DEVICE_H
