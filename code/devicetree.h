#ifndef DEVICETREE_H
#define DEVICETREE_H

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

#define DEVICETREE_MAX_PATH_LEN 256

#define devicetree_u32(ptr) ((u32)((((u8 *)ptr)[0] << 24) | (((u8 *)ptr)[1] << 16) | (((u8 *)ptr)[2] << 8) | (((u8 *)ptr)[3])))

#define DEVICETREE_CALLBACK(name) b32 name(c8 *path, c8 *prop_name, u8 *prop, um32 prop_size, void *userdata)
typedef DEVICETREE_CALLBACK(DevicetreeCallback);

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

#endif //DEVICETREE_H
