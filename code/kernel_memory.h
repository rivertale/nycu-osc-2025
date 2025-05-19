#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#define KERNEL_DIRECT_MAP_OFFSET KERNEL_SPACE_OFFSET

#define MAX_PHYSICAL_MEMORY_REGION_COUNT 64

#define PAGE_EXPONENT PAGE_MIN_ALLOC_EXPONENT
#define PAGE_SIZE PAGE_MIN_ALLOC_SIZE

#define PAGE_MIN_ALLOC_EXPONENT 12 // 4kb (2^12) allocation
#define PAGE_MAX_ALLOC_EXPONENT 22 // 4mb (2^22) allocation
#define PAGE_MAX_ALLOC_ORDER (PAGE_MAX_ALLOC_EXPONENT - PAGE_MIN_ALLOC_EXPONENT)
#define PAGE_MIN_ALLOC_SIZE (1 << PAGE_MIN_ALLOC_EXPONENT)
#define PAGE_MAX_ALLOC_SIZE (1 << PAGE_MAX_ALLOC_EXPONENT)

#define SLAB_MAX_ALLOC_EXPONENT 11
#define SLAB_MAX_ALLOC_SIZE (1 << SLAB_MAX_ALLOC_EXPONENT)
#define SLAB_SMALL_GRANULARITY_EXPONENT 4
#define SLAB_SMALL_GRANULARITY (1 << SLAB_SMALL_GRANULARITY_EXPONENT)
#define SLAB_SMALL_MAX_ALLOC_SIZE \
    (SLAB_SMALL_GRANULARITY * (array_count(g_small_size_to_slab_index_map) - 1))

#define PAGE_TABLE_SIZE PAGE_SIZE

#define PAGE_ADDR_MASK 0x0000fffffffff000ull
#define PAGE_ATTRIB_RW_MASK (3ull << 6)

#define PAGE_ATTRIB_TABLE (3ull << 0)
#define PAGE_ATTRIB_L3_PAGE (3ull << 0)
#define PAGE_ATTRIB_PAGE (1ull << 0)
#define PAGE_ATTRIB_MAIR_INDEX_NORMAL (0ull << 2)
#define PAGE_ATTRIB_MAIR_INDEX_DEVICE (1ull << 2)
#define PAGE_ATTRIB_RW_EL1 (0ull << 6)
#define PAGE_ATTRIB_RW_EL0 (1ull << 6)
#define PAGE_ATTRIB_RO_EL1 (2ull << 6)
#define PAGE_ATTRIB_RO_EL0 (3ull << 6)
#define PAGE_ATTRIB_ACCESS (1ull << 10)
#define PAGE_ATTRIB_PXN (1ull << 53)
#define PAGE_ATTRIB_UXN (1ull << 54)

#define VIRTUAL_MEMORY_FLAG_MASK 0x3f
#define VIRTUAL_MEMORY_FLAG_COLOR 0x1
#define VIRTUAL_MEMORY_FLAG_MAPPED 0x2
#define VIRTUAL_MEMORY_FLAG_DEVICE 0x4
#define VIRTUAL_MEMORY_FLAG_READ 0x8
#define VIRTUAL_MEMORY_FLAG_WRITE 0x10
#define VIRTUAL_MEMORY_FLAG_EXECUTE 0x20

#define VIRTUAL_MEMORY_COLOR_RED 0x0
#define VIRTUAL_MEMORY_COLOR_BLACK 0x1


typedef enum AllocationType
{
    AllocationType_demand = 0,
    AllocationType_commit = 1,
} AllocationType;

typedef enum MemoryPermission
{
    MemoryPermission_none = 0x0,
    MemoryPermission_read = 0x1,
    MemoryPermission_write = 0x2,
    MemoryPermission_execute = 0x4,
} MemoryPermission;

typedef union Slab Slab;

typedef struct PhysicalMemoryRegion
{
    u64 low;
    u64 high;
} PhysicalMemoryRegion;

typedef struct PhysicalMemoryRegionList
{
    u32 count;
    __attribute__((aligned(16))) PhysicalMemoryRegion regions[MAX_PHYSICAL_MEMORY_REGION_COUNT];
} PhysicalMemoryRegionList;

typedef struct BootArena
{
    umm used;
    umm max;
    u8 *memory;
} BootArena;

typedef struct L3PageEntryIter
{
    s32 l0_index;
    s32 l1_index;
    s32 l2_index;
    s32 l3_index;
    u64 *page_table;
    u64 *entry;
} L3PageEntryIter;

typedef struct PageInfo
{
    u32 reference_count; // NOTE: only l1, l2, l3 page tables and user pages use reference_count
    Slab *slab;
} PageInfo;

typedef struct PageBlock
{
    struct PageBlock *prev;
    struct PageBlock *next;
} PageBlock;

typedef struct PagePool
{
    s32 next_exponent_after_page_count;
    u8 *base;
    u64 *reservation;

    u64 page_count;
    PageInfo *page_infos;

    PageBlock free_block[PAGE_MAX_ALLOC_ORDER + 1];
} PagePool;

typedef struct SlabAllocation
{
    struct SlabAllocation *next_free;
} SlabAllocation;

typedef struct SlabLink
{
    struct SlabLink *prev;
    struct SlabLink *next;
} SlabLink;

typedef union Slab
{
    union Slab *next_free;
    struct
    {
        // NOTE: link must be the first field
        SlabLink link;
        u32 slab_index;
        u16 allocation_count;
        u16 max_allocation_count;
        SlabAllocation *first_free_allocation;
        u8 *memory;
    };
} Slab;

typedef struct MemoryAllocator
{
    Slab *first_free_slab;
    SlabLink full_slab[SLAB_MAX_ALLOC_EXPONENT + 1];
    SlabLink partial_slab[SLAB_MAX_ALLOC_EXPONENT + 1];
} MemoryAllocator;

typedef struct VirtualMemoryNode
{
    umm low, high;
    umm parent_and_flags;
    
    void *file;
    u64 file_offset;
    
    struct VirtualMemoryNode *lhs;
    struct VirtualMemoryNode *rhs;
} VirtualMemoryNode;

// NOTE: distribute virtual address for incoming allocation, the data structure is a red-black tree
// and each node is a virtual memory allocation
typedef struct VirtualMemoryTree
{
    umm lowest;
    umm highest;
    VirtualMemoryNode *root;
} VirtualMemoryTree;

typedef struct EmptyVirtualMemoryRange
{
    umm addr;
    VirtualMemoryNode *parent;
    VirtualMemoryNode **link;
} EmptyVirtualMemoryRange;

// size:       0, 0, 16, 32, 64, 96, 128, 192, 256, 512, 1024, 2048
// slab_index: 0, 1,  2,  3,  4,  5,   6,   7,   8,   9,   10,   11
static s32 g_small_size_to_slab_index_map[] =
{
    0,
    2, // SLAB_SMALL_GRANULARITY * 1
    3, // SLAB_SMALL_GRANULARITY * 2
    4, // SLAB_SMALL_GRANULARITY * 3
    4, // SLAB_SMALL_GRANULARITY * 4
    5, // SLAB_SMALL_GRANULARITY * 5
    5, // SLAB_SMALL_GRANULARITY * 6
    6, // SLAB_SMALL_GRANULARITY * 7
    6, // SLAB_SMALL_GRANULARITY * 8
    7, // SLAB_SMALL_GRANULARITY * 9
    7, // SLAB_SMALL_GRANULARITY * 10
    7, // SLAB_SMALL_GRANULARITY * 11
    7, // SLAB_SMALL_GRANULARITY * 12
};

static u32 g_slab_index_to_size_map[] =
{
    0,
    0,
    16, // SLAB_SMALL_GRANULARITY * 1
    32, // SLAB_SMALL_GRANULARITY * 2
    64, // SLAB_SMALL_GRANULARITY * 4
    96, // SLAB_SMALL_GRANULARITY * 6
    128, // SLAB_SMALL_GRANULARITY * 8
    192, // SLAB_SMALL_GRANULARITY * 12
    256,
    512,
    1024,
    2048,
};
#endif //KERNEL_MEMORY_H
