#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

#define MAX_MEMORY_REGION_COUNT 64

#define PAGE_EXPONENT PAGE_MIN_ALLOC_EXPONENT
#define PAGE_SIZE PAGE_MIN_ALLOC_SIZE

#define PAGE_MIN_ALLOC_EXPONENT 12 // 4kb (2^12) allocation
#define PAGE_MAX_ALLOC_EXPONENT 22 // 4mb (2^22) allocation
#define PAGE_MAX_ALLOC_ORDER (PAGE_MAX_ALLOC_EXPONENT - PAGE_MIN_ALLOC_EXPONENT)
#define PAGE_MIN_ALLOC_SIZE (1 << PAGE_MIN_ALLOC_EXPONENT)
#define PAGE_MAX_ALLOC_SIZE (1 << PAGE_MAX_ALLOC_EXPONENT)

#define ALLOCATOR_MAX_SLAB_EXPONENT 11
#define ALLOCATOR_MAX_SLAB_SIZE (1 << ALLOCATOR_MAX_SLAB_EXPONENT)
#define ALLOCATOR_MAX_MAPPED_SIZE \
    (ALLOCATOR_MAP_GRANULARITY * array_count(g_allocator_size_to_cache_index_map))
#define ALLOCATOR_MAP_GRANULARITY_EXPONENT 4
#define ALLOCATOR_MAP_GRANULARITY (1 << ALLOCATOR_MAP_GRANULARITY_EXPONENT)

typedef union Slab Slab;

typedef struct MemoryRegion
{
    void *low;
    void *high;
} MemoryRegion;

typedef struct MemoryRegionList
{
    u32 count;
    MemoryRegion regions[MAX_MEMORY_REGION_COUNT];
} MemoryRegionList;

typedef struct BootArena
{
    umm used;
    umm max;
    u8 *memory;
} BootArena;

typedef struct PageInfo
{
    Slab *slab;
} PageInfo;

typedef struct PageBlock
{
    struct PageBlock *prev;
    struct PageBlock *next;
} PageBlock;

typedef struct PagePool
{
    s32 next_exponent_after_max_page;
    u8 *base;
    u64 *reservation;
    
    u64 max_page;
    PageInfo *page_infos;
    
    PageBlock free_block[PAGE_MAX_ALLOC_ORDER];
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
        s32 cache_index;
        u16 allocation_count;
        u16 max_allocation_count;
        SlabAllocation *first_free_allocation;
        u8 *memory;
    };
} Slab;

// size:        0, 0, 16, 32, 64, 96, 128, 192, 256, 512, 1024, 2048
// cache_index: 0, 1, 2,  3,  4,  5,  6,   7,   8,   9,    10,  11
static s32 g_allocator_size_to_cache_index_map[] =
{
    2, // ALLOCATOR_MAP_GRANULARITY * 1
    3, // ALLOCATOR_MAP_GRANULARITY * 2
    4, // ALLOCATOR_MAP_GRANULARITY * 3
    4, // ALLOCATOR_MAP_GRANULARITY * 4
    5, // ALLOCATOR_MAP_GRANULARITY * 5
    5, // ALLOCATOR_MAP_GRANULARITY * 6
    6, // ALLOCATOR_MAP_GRANULARITY * 7
    6, // ALLOCATOR_MAP_GRANULARITY * 8
    7, // ALLOCATOR_MAP_GRANULARITY * 9
    7, // ALLOCATOR_MAP_GRANULARITY * 10
    7, // ALLOCATOR_MAP_GRANULARITY * 11
    7, // ALLOCATOR_MAP_GRANULARITY * 12
};

typedef struct MemoryAllocator
{
    Slab *first_free_slab;
    PagePool *page_pool;
    SlabLink full_cache[ALLOCATOR_MAX_SLAB_EXPONENT + 1];
    SlabLink partial_cache[ALLOCATOR_MAX_SLAB_EXPONENT + 1];
} MemoryAllocator;

#endif //KERNEL_MEMORY_H
