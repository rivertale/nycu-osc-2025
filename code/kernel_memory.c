#define direct_mapped_physical_address(virtual_addr) \
((u64)((u8 *)(virtual_addr) - KERNEL_DIRECT_MAP_OFFSET))
#define direct_mapped_virtual_address(physical_addr) \
((void *)((u64)(physical_addr) + KERNEL_DIRECT_MAP_OFFSET))

static void yield_physical_thread(void);
static void exit_process(Process *process, ExitCode exit_code);

#if 0
static void *
push_size(BootArena *arena, umm size)
{
    void *result = 0;
    umm aligned_size = align_up(size, 16);
    if(arena->used + aligned_size <= arena->max)
    {
        result = arena->memory + arena->used;
        arena->used += aligned_size;
    }
    return result;
}

static BootArena *
bootstrap_boot_arena(void *low, void *high)
{
    BootArena *arena = 0;
    assert(low != 0);
    
    low = (void *)align_up((u64)low, 16);
    high = (void *)align_down((u64)high, 16);
    um32 bootstrap_size = align_up(sizeof(*arena), 16);
    
    u8 *memory = (u8 *)low;
    umm size = (u8 *)high - (u8 *)low;
    if(size >= bootstrap_size)
    {
        arena = (BootArena *)memory;
        clear_memory(arena, sizeof(*arena));
        arena->memory = memory + bootstrap_size;
        arena->max = size - bootstrap_size;
    }
    return arena;
}
#endif

static void
reserve_physical_memory_region(PhysicalMemoryRegionList *list, u64 low, u64 high)
{
    assert(low <= high);
    
    u32 cur = 0;
    for(u32 index = 0; index < list->count; ++index)
    {
        PhysicalMemoryRegion *region = list->regions + index;
        if(region->low < low)
        {
            if(region->high < low)
            {
                list->regions[cur].low = region->low;
                list->regions[cur].high = region->high;
                ++cur;
            }
            else if(region->high < high)
            {
                list->regions[cur].low = region->low;
                list->regions[cur].high = low;
                ++cur;
            }
            else
            {
                // NOTE: won't overlap with other regions, split the region and terminate
                assert(list->count < MAX_PHYSICAL_MEMORY_REGION_COUNT);
                for(u32 i = list->count - 1; i > index; --i)
                    list->regions[i + 1] = list->regions[i];
                
                u64 low0 = region->low;
                u64 high0 = low;
                u64 low1 = high;
                u64 high1 = region->high;
                region[0].low = low0;
                region[0].high = high0;
                region[1].low = low1;
                region[1].high = high1;
                cur = list->count + 1;
                break;
            }
        }
        else if(region->low < high)
        {
            if(region->high < high)
            {
                // delete region
                do_nothing;
            }
            else
            {
                
                list->regions[cur].low = high;
                list->regions[cur].high = region->high;
                ++cur;
            }
        }
        else
        {
            list->regions[cur].low = region->low;
            list->regions[cur].high = region->high;
            ++cur;
        }
    }
    
    list->count = cur;
}

static void
init_physical_memory_region_list(PhysicalMemoryRegionList *list, u64 lowest, u64 highest)
{
    assert(lowest <= highest);
    
    list->count = 0;
    
    assert(list->count < MAX_PHYSICAL_MEMORY_REGION_COUNT);
    PhysicalMemoryRegion *region = list->regions + list->count++;
    region->low = lowest;
    region->high = highest;
}

static PageInfo *
get_page_info(void *page)
{
    PagePool *pool = &g_kernel_state.page_pool;
    u64 page_index = ((u8 *)page - pool->base) >> PAGE_EXPONENT;
    return pool->page_infos + page_index;
}

static u64
page_pool_get_reservation_bit_index(PagePool *pool, s32 order, void *block)
{
    // NOTE: bit index if block index is 00000000 - 11111111 (next_exponent_after_page_count=8)
    // 000000000 - 011111111 (order 0)
    // 100000000 - 101111111 (order 1)
    // 110000000 - 110111111 (order 2)
    
    assert((void *)pool->base <= block);
    u64 mask = (1ull << (pool->next_exponent_after_page_count + 1)) - 1;
    u64 block_index = ((u8 *)block - pool->base) >> (PAGE_MIN_ALLOC_EXPONENT + order);
    u64 order_bits = (-1ull << (pool->next_exponent_after_page_count - order + 1)) & mask;
    
    u64 reservation_count = next_power_of_two(pool->page_count) << 1;
    assert((order_bits | block_index) < reservation_count);
    return order_bits | block_index;
}

static b32
page_pool_is_reserved(PagePool *pool, s32 order, void *block)
{
    u64 bit_index = page_pool_get_reservation_bit_index(pool, order, block);
    
    // NOTE: careful, reservation is u64, but b32 is only 32-bit
    b32 result = ((pool->reservation[bit_index >> 6] & (1ull << (bit_index & 63))) != 0);
    return result;
}

static void
page_pool_mark_reserved(PagePool *pool, s32 order, void *block)
{
    u64 bit_index = page_pool_get_reservation_bit_index(pool, order, block);
    pool->reservation[bit_index >> 6] |= (1ull << (bit_index & 63));
}

static void
page_pool_unmark_reserved(PagePool *pool, s32 order, void *block)
{
    u64 bit_index = page_pool_get_reservation_bit_index(pool, order, block);
    pool->reservation[bit_index >> 6] &= ~(1ull << (bit_index & 63));
}

static s32
page_pool_calculate_maximum_order(u64 physical_address)
{
    s32 result = find_least_significant_bit(physical_address) - PAGE_MIN_ALLOC_EXPONENT;
    if(result > PAGE_MAX_ALLOC_ORDER)
        result = PAGE_MAX_ALLOC_ORDER;
    
    return result;
}

static void
free_pages(void *ptr)
{
    if(!ptr)
        return;
    
    PagePool *pool = &g_kernel_state.page_pool;
    PageBlock *block = (PageBlock *)ptr;
    
    s32 order = 0;
    while(order <= PAGE_MAX_ALLOC_ORDER)
    {
        if(page_pool_is_reserved(pool, order, block))
            break;
        ++order;
    }
    
    if(order <= PAGE_MAX_ALLOC_ORDER)
    {
        // NOTE: we are not sure if we are freeing valid memory
        page_pool_unmark_reserved(pool, order, block);
        
        while(order < PAGE_MAX_ALLOC_ORDER)
        {
            s32 exponent = PAGE_MIN_ALLOC_EXPONENT + order;
            
            PageBlock *buddy = (PageBlock *)((u64)block ^ (1 << exponent));
            if(page_pool_is_reserved(pool, order, buddy))
                break;
            
            page_pool_unmark_reserved(pool, order, block);
            double_link_remove(buddy);
            
            block = (PageBlock *)((u64)block & ~(1 << exponent));
            ++order;
        }
        double_link_insert_at_last(&pool->free_block[order], block);
    }
    else
    {
        debug_log("free invalid memory");
    }
}

static void *
alloc_pages(umm size)
{
    PagePool *pool = &g_kernel_state.page_pool;
    
    void *result = 0;
    if(0 < size && size <= PAGE_MAX_ALLOC_SIZE)
    {
        if(size < PAGE_MIN_ALLOC_SIZE)
            size = PAGE_MIN_ALLOC_SIZE;
        size = next_power_of_two(size);
        
        s32 order = find_most_significant_bit(size) - PAGE_MIN_ALLOC_EXPONENT;
        s32 split_order = order;
        while(split_order <= PAGE_MAX_ALLOC_ORDER)
        {
            if(!double_link_is_empty(&pool->free_block[split_order]))
                break;
            ++split_order;
        }
        
        if(split_order < PAGE_MAX_ALLOC_ORDER)
        {
            // NOTE: split the block until it fits the allocation size
            while(split_order > order)
            {
                PageBlock *block = pool->free_block[split_order].next;
                double_link_remove(block);
                page_pool_mark_reserved(pool, split_order, block);
                
                --split_order;
                um32 split_size = 1 << (PAGE_MIN_ALLOC_EXPONENT + split_order);
                
                PageBlock *low_block = (PageBlock *)((u8 *)block);
                PageBlock *high_block = (PageBlock *)((u8 *)block + split_size);
                double_link_insert_at_last(&pool->free_block[split_order], low_block);
                double_link_insert_at_last(&pool->free_block[split_order], high_block);
            }
            
            result = (void *)pool->free_block[split_order].next;
            double_link_remove((PageBlock *)result);
            page_pool_mark_reserved(pool, order, result);
            
            clear_memory(result, size);
        }
        else
        {
            debug_log("out of memory");
        }
    }
    
    return result;
}

static void
init_page_pool(PagePool *pool, PhysicalMemoryRegionList *region_list)
{
    clear_memory(pool, sizeof(*pool));
    for(s32 order = 0; order <= PAGE_MAX_ALLOC_ORDER; ++order)
    {
        double_link_init(&pool->free_block[order]);
    }
    
    if(region_list->count == 0)
        return;
    
    PhysicalMemoryRegion *first_region = region_list->regions;
    PhysicalMemoryRegion *last_region = region_list->regions + region_list->count - 1;
    u64 lowest = align_down(first_region->low, PAGE_MAX_ALLOC_SIZE);
    u64 highest = align_up(last_region->high, PAGE_MAX_ALLOC_SIZE);
    
    u64 page_count = (highest - lowest) >> PAGE_MIN_ALLOC_EXPONENT;
    u64 reservation_count = next_power_of_two(page_count) << 1;
    
    u64 page_info_size = page_count * sizeof(PageInfo);
    u64 reservation_size = align_up(reservation_count, 64) >> 3;
    
    pool->next_exponent_after_page_count = find_most_significant_bit(next_power_of_two(page_count));
    pool->page_count = page_count;
    pool->base = (u8 *)direct_mapped_virtual_address(lowest);
    
    // NOTE: reserve memory for page info array
    for(s32 index = 0; index < region_list->count; ++index)
    {
        PhysicalMemoryRegion *region = region_list->regions + index;
        if(region->high - region->low >= page_info_size)
        {
            pool->page_infos = (PageInfo *)direct_mapped_virtual_address(region->low);
            region->low += page_info_size;
            break;
        }
    }
    clear_memory(pool->page_infos, page_info_size);
    
    // NOTE: reserve memory for reservation bit array
    for(s32 index = 0; index < region_list->count; ++index)
    {
        PhysicalMemoryRegion *region = region_list->regions + index;
        if(region->high - region->low >= reservation_size)
        {
            pool->reservation = (u64 *)direct_mapped_virtual_address(region->low);
            region->low += reservation_size;
            break;
        }
    }
    clear_memory(pool->reservation, reservation_size);
    
    for(s32 index = 0; index < region_list->count; ++index)
    {
        PhysicalMemoryRegion *region = region_list->regions + index;
        
        u64 low = align_up(region->low, PAGE_MIN_ALLOC_SIZE);
        u64 high = align_down(region->high, PAGE_MIN_ALLOC_SIZE);
        s32 low_order = page_pool_calculate_maximum_order(low);
        s32 high_order = page_pool_calculate_maximum_order(high);
        
        while(low < high)
        {
            assert(0 <= low_order && low_order <= PAGE_MAX_ALLOC_ORDER);
            assert(0 <= high_order && high_order <= PAGE_MAX_ALLOC_ORDER);
            
            if(low_order <= high_order)
            {
                u32 block_size = 1 << (low_order + PAGE_MIN_ALLOC_EXPONENT);
                
                PageBlock *block = (PageBlock *)direct_mapped_virtual_address(low);
                double_link_insert_at_last(&pool->free_block[low_order], block);
                assert(block);
                
                low += block_size;
                low_order = page_pool_calculate_maximum_order(low);
            }
            else
            {
                u32 block_size = 1 << (high_order + PAGE_MIN_ALLOC_EXPONENT);
                
                PageBlock *block = (PageBlock *)direct_mapped_virtual_address(high - block_size);
                double_link_insert_at_last(&pool->free_block[high_order], block);
                
                high -= block_size;
                high_order = page_pool_calculate_maximum_order(high);
            }
        }
    }
    
    for(u64 addr = lowest; addr < highest; addr += PAGE_MAX_ALLOC_SIZE)
    {
        PageBlock *block = (PageBlock *)direct_mapped_virtual_address(addr);
        page_pool_mark_reserved(pool, PAGE_MAX_ALLOC_ORDER, block);
    }
    
    for(s32 free_order = 0; free_order <= PAGE_MAX_ALLOC_ORDER; ++free_order)
    {
        for(PageBlock *free_block = pool->free_block[free_order].next;
            free_block != &pool->free_block[free_order];
            free_block = free_block->next)
        {
            PageBlock *block = free_block;
            for(s32 order = free_order; order < PAGE_MAX_ALLOC_ORDER; ++order)
            {
                u32 block_size = 1 << (order + PAGE_MIN_ALLOC_EXPONENT);
                PageBlock *buddy = (PageBlock *)((u64)block ^ block_size);
                page_pool_mark_reserved(pool, order, block);
                page_pool_mark_reserved(pool, order, buddy);
                block = (PageBlock *)((u64)block & ~(u64)block_size);
            }
            page_pool_mark_reserved(pool, PAGE_MAX_ALLOC_ORDER, block);
        }
    }
    
    for(s32 order = 0; order <= PAGE_MAX_ALLOC_ORDER; ++order)
    {
        for(PageBlock *block = pool->free_block[order].next;
            block != &pool->free_block[order];
            block = block->next)
        {
            page_pool_unmark_reserved(pool, order, block);
        }
    }
}

static Slab *
create_slab(MemoryAllocator *allocator, u32 slab_index, umm allocation_size)
{
    if(!allocator->first_free_slab)
    {
        u8 *memory = (u8 *)alloc_pages(PAGE_SIZE);
        assert(memory);
        
        for(um32 offset = 0; offset + sizeof(Slab) <= PAGE_SIZE; offset += sizeof(Slab))
        {
            Slab *slab = (Slab *)(memory + offset);
            slab->next_free = allocator->first_free_slab;
            allocator->first_free_slab = slab;
        }
    }
    
    assert(allocator->first_free_slab);
    Slab *slab = allocator->first_free_slab;
    allocator->first_free_slab = slab->next_free;
    
    clear_memory(slab, sizeof(*slab));
    
    slab->memory = alloc_pages(PAGE_SIZE);
    slab->slab_index = slab_index;
    slab->allocation_count = 0;
    slab->max_allocation_count = PAGE_SIZE / allocation_size;
    assert(slab->memory);
    
    PageInfo *page_info = get_page_info(slab->memory);
    page_info->slab = slab;
    
    for(um32 offset = 0; offset + allocation_size <= PAGE_SIZE; offset += allocation_size)
    {
        SlabAllocation *allocation = (SlabAllocation *)(slab->memory + offset);
        allocation->next_free = slab->first_free_allocation;
        slab->first_free_allocation = allocation;
    }
    
    double_link_insert_at_last(&allocator->partial_slab[slab_index], &slab->link);
    return slab;
}

static u32
get_slab_index(umm size)
{
    u32 result = 0;
    
    assert(size <= SLAB_MAX_ALLOC_SIZE);
    if(size <= SLAB_SMALL_MAX_ALLOC_SIZE)
    {
        size = align_up(size, SLAB_SMALL_GRANULARITY);
        result = g_small_size_to_slab_index_map[size >> SLAB_SMALL_GRANULARITY_EXPONENT];
    }
    else
    {
        size = next_power_of_two(size);
        result = find_most_significant_bit(size);
    }
    return result;
}

static void
free_kernel_memory(void *ptr)
{
    if(!ptr)
        return;
    
    MemoryAllocator *allocator = &g_kernel_state.allocator;
    
    u8 *page_boundary = (u8 *)align_down((u64)ptr, PAGE_SIZE);
    PageInfo *page_info = get_page_info(page_boundary);
    
    Slab *slab = page_info->slab;
    if(slab)
    {
        assert(slab->allocation_count > 0);
        
        --slab->allocation_count;
        if(slab->allocation_count == 0)
        {
            page_info->slab = 0;
            free_pages(slab->memory);
            
            double_link_remove(&slab->link);
            slab->next_free = allocator->first_free_slab;
            allocator->first_free_slab = slab;
        }
        else
        {
            if(slab->allocation_count == slab->max_allocation_count - 1)
            {
                double_link_remove(&slab->link);
                double_link_insert_at_last(&allocator->partial_slab[slab->slab_index], &slab->link);
            }
            SlabAllocation *allocation = (SlabAllocation *)ptr;
            allocation->next_free = slab->first_free_allocation;
            slab->first_free_allocation = allocation;
        }
    }
    else
    {
        free_pages(page_boundary);
    }
}

static void *
alloc_kernel_memory(umm size)
{
    MemoryAllocator *allocator = &g_kernel_state.allocator;
    
    void *result = 0;
    if(size > 0)
    {
        if(size <= SLAB_MAX_ALLOC_SIZE)
        {
            u32 slab_index = get_slab_index(size);
            size = g_slab_index_to_size_map[slab_index];
            
            if(double_link_is_empty(&allocator->partial_slab[slab_index]))
                create_slab(allocator, slab_index, size);
            
            Slab *slab = (Slab *)allocator->partial_slab[slab_index].next;
            assert(slab);
            
            result = (void *)slab->first_free_allocation;
            slab->first_free_allocation = ((SlabAllocation *)result)->next_free;
            assert(result);
            
            ++slab->allocation_count;
            if(slab->allocation_count == slab->max_allocation_count)
            {
                double_link_remove(&slab->link);
                double_link_insert_at_last(&allocator->full_slab[slab_index], &slab->link);
            }
            
            clear_memory(result, size);
        }
        else
        {
            result = alloc_pages(size);
        }
    }
    return result;
}

static void
init_memory_allocator(MemoryAllocator *allocator)
{
    clear_memory(allocator, sizeof(*allocator));
    for(s32 index = 0; index <= SLAB_MAX_ALLOC_EXPONENT; ++index)
    {
        double_link_init(&allocator->full_slab[index]);
        double_link_init(&allocator->partial_slab[index]);
    }
}

static b32
is_lhs_virtual_memory_node(VirtualMemoryNode *parent, VirtualMemoryNode *child)
{
    return parent->lhs == child;
}

static b32
is_rhs_virtual_memory_node(VirtualMemoryNode *parent, VirtualMemoryNode *child)
{
    return parent->rhs == child;
}

static VirtualMemoryNode *
virtual_memory_node_parent(VirtualMemoryNode *node)
{
    return (VirtualMemoryNode *)(node->parent_and_flags & ~VIRTUAL_MEMORY_FLAG_MASK);
}

static umm
virtual_memory_node_color(VirtualMemoryNode *node)
{
    return node->parent_and_flags & VIRTUAL_MEMORY_FLAG_COLOR;
}

static void
set_virtual_memory_node_color(VirtualMemoryNode *node, umm color)
{
    node->parent_and_flags = (node->parent_and_flags & ~VIRTUAL_MEMORY_FLAG_COLOR) | color;
}

static void
set_virtual_memory_node_parent(VirtualMemoryNode *node, VirtualMemoryNode *parent)
{
    node->parent_and_flags = (umm)parent | (node->parent_and_flags & VIRTUAL_MEMORY_FLAG_MASK);
}

static void
set_virtual_memory_node_flags_except_color(VirtualMemoryNode *node, u64 parent_and_flags)
{
    u64 mask = VIRTUAL_MEMORY_FLAG_MASK ^ VIRTUAL_MEMORY_FLAG_COLOR;
    node->parent_and_flags = (node->parent_and_flags & ~mask) | (parent_and_flags & mask);
}

static VirtualMemoryNode *
next_virtual_memory_node(VirtualMemoryNode *node)
{
    VirtualMemoryNode *result = 0;
    if(node->rhs)
    {
        node = node->rhs;
        while(node->lhs)
            node = node->lhs;
        
        result = node;
    }
    else
    {
        for(;;)
        {
            VirtualMemoryNode *parent = virtual_memory_node_parent(node);
            if(!parent)
                break;
            
            if(is_lhs_virtual_memory_node(parent, node))
            {
                result = parent;
                break;
            }
            node = parent;
        }
    }
    return result;
}

static void
rotate_virtual_memory_tree_clockwise(VirtualMemoryTree *tree, VirtualMemoryNode *node)
{
    VirtualMemoryNode *parent = virtual_memory_node_parent(node);
    VirtualMemoryNode *child = node->lhs;
    
    node->lhs = child->rhs;
    child->rhs = node;
    if(!parent)
        tree->root = child;
    else if(is_lhs_virtual_memory_node(parent, node))
        parent->lhs = child;
    else // is_rhs_virtual_memory_node(parent, node)
        parent->rhs = child;
    
    set_virtual_memory_node_parent(node, child);
    if(node->lhs)
        set_virtual_memory_node_parent(node->lhs, node);
    set_virtual_memory_node_parent(child, parent);
}

static void
rotate_virtual_memory_tree_counterclockwise(VirtualMemoryTree *tree, VirtualMemoryNode *node)
{
    VirtualMemoryNode *parent = virtual_memory_node_parent(node);
    VirtualMemoryNode *child = node->rhs;
    
    node->rhs = child->lhs;
    child->lhs = node;
    if(!parent)
        tree->root = child;
    else if(is_lhs_virtual_memory_node(parent, node))
        parent->lhs = child;
    else // is_rhs_virtual_memory_node(parent, node)
        parent->rhs = child;
    
    set_virtual_memory_node_parent(node, child);
    if(node->rhs)
        set_virtual_memory_node_parent(node->rhs, node);
    set_virtual_memory_node_parent(child, parent);
}

static EmptyVirtualMemoryRange
find_empty_virtual_memory_range(VirtualMemoryTree *tree, umm size)
{
    EmptyVirtualMemoryRange result = {0};
    
    if(!tree->root)
    {
        result.addr = tree->lowest;
        result.link = &tree->root;
        result.parent = 0;
    }
    else
    {
        VirtualMemoryNode *prev_node = 0;
        VirtualMemoryNode *node = tree->root;
        while(node->lhs)
            node = node->lhs;
        
        umm low = tree->lowest;
        while(node)
        {
            if(low + size <= node->low)
                break;
            
            low = node->high;
            prev_node = node;
            node = next_virtual_memory_node(node);
        }
        
        if(low + size <= tree->highest)
        {
            result.addr = low;
            if(node && !node->lhs)
            {
                result.link = &node->lhs;
                result.parent = node;
            }
            else
            {
                assert(prev_node);
                result.link = &prev_node->rhs;
                result.parent = prev_node;
            }
        }
    }
    return result;
}

static EmptyVirtualMemoryRange
get_empty_virtual_memory_range(VirtualMemoryTree *tree, umm addr, umm size)
{
    EmptyVirtualMemoryRange result = {0};
    if(!tree->root)
    {
        if(tree->lowest <= addr && addr + size <= tree->highest)
        {
            result.addr = addr;
            result.link = &tree->root;
            result.parent = 0;
        }
    }
    else
    {
        VirtualMemoryNode *parent = 0;
        VirtualMemoryNode *node = tree->root;
        while(node)
        {
            if(addr < node->low)
            {
                parent = node;
                node = node->lhs;
            }
            else if(node->high <= addr)
            {
                parent = node;
                node = node->rhs;
            }
            else
            {
                // failed, overlapped memory
                parent = 0;
                break;
            }
        }
        
        if(parent)
        {
            if(parent->high <= addr)
            {
                assert(!parent->rhs);
                result.addr = addr;
                result.link = &parent->rhs;
                result.parent = parent;
            }
            else if(addr + size <= parent->low)
            {
                assert(!parent->lhs);
                result.addr = addr;
                result.link = &parent->lhs;
                result.parent = parent;
            }
            else
            {
                invalid_code_path;
            }
        }
    }
    return result;
}

static VirtualMemoryNode *
find_virtual_memory_node(VirtualMemoryTree *tree, void *user_addr)
{
    VirtualMemoryNode *result = 0;
    VirtualMemoryNode *node = tree->root;
    while(node)
    {
        if(node->low <= (umm)user_addr && (umm)user_addr < node->high)
        {
            result = node;
            break;
        }
        else if((umm)user_addr < node->low)
        {
            node = node->lhs;
        }
        else if(node->high <= (umm)user_addr)
        {
            node = node->rhs;
        }
        else
        {
            break;
        }
    }
    return result;
}

static void
fixup_virtual_memory_node_insertion(VirtualMemoryTree *tree, VirtualMemoryNode *node)
{
    for(;;)
    {
        VirtualMemoryNode *parent = virtual_memory_node_parent(node);
        if(!parent)
        {
            set_virtual_memory_node_parent(node, 0);
            set_virtual_memory_node_color(node, VIRTUAL_MEMORY_COLOR_BLACK);
            break;
        }
        
        if(virtual_memory_node_color(parent) == VIRTUAL_MEMORY_COLOR_BLACK)
            break;
        
        // NOTE: since parent is red, it is not root, we must have a black grandparent
        VirtualMemoryNode *grandparent = virtual_memory_node_parent(parent);
        VirtualMemoryNode *uncle = is_lhs_virtual_memory_node(grandparent, parent) ?
            grandparent->rhs : grandparent->lhs;
        
        if(uncle && virtual_memory_node_color(uncle) == VIRTUAL_MEMORY_COLOR_RED)
        {
            set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
            set_virtual_memory_node_color(uncle, VIRTUAL_MEMORY_COLOR_BLACK);
            set_virtual_memory_node_color(grandparent, VIRTUAL_MEMORY_COLOR_RED);
            node = grandparent;
        }
        else if(is_lhs_virtual_memory_node(grandparent, parent))
        {
            if(is_rhs_virtual_memory_node(parent, node))
            {
                rotate_virtual_memory_tree_counterclockwise(tree, parent);
                node = parent;
                parent = virtual_memory_node_parent(node);
            }
            
            set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
            set_virtual_memory_node_color(grandparent, VIRTUAL_MEMORY_COLOR_RED);
            rotate_virtual_memory_tree_clockwise(tree, grandparent);
            break;
        }
        else // is_rhs_virtual_memory_node(grandparent, parent)
        {
            if(is_lhs_virtual_memory_node(parent, node))
            {
                rotate_virtual_memory_tree_clockwise(tree, parent);
                node = parent;
                parent = virtual_memory_node_parent(node);
            }
            
            set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
            set_virtual_memory_node_color(grandparent, VIRTUAL_MEMORY_COLOR_RED);
            rotate_virtual_memory_tree_counterclockwise(tree, grandparent);
            break;
        }
    }
}

static VirtualMemoryNode *
map_virtual_memory_node(VirtualMemoryTree *tree, umm addr, umm size)
{
    size = align_up(size, PAGE_SIZE);
    
    VirtualMemoryNode *result = 0;
    EmptyVirtualMemoryRange empty_range = get_empty_virtual_memory_range(tree, addr, size);
    if(empty_range.link)
    {
        result = alloc_kernel_memory(sizeof(*result));
        result->low = empty_range.addr;
        result->high = empty_range.addr + size;
        result->parent_and_flags = (umm)empty_range.parent | VIRTUAL_MEMORY_COLOR_RED;
        *empty_range.link = result;
        fixup_virtual_memory_node_insertion(tree, result);
    }
    return result;
}

static VirtualMemoryNode *
insert_virtual_memory_node(VirtualMemoryTree *tree, umm size)
{
    size = align_up(size, PAGE_SIZE);
    
    VirtualMemoryNode *result = 0;
    EmptyVirtualMemoryRange empty_range = find_empty_virtual_memory_range(tree, size);
    if(empty_range.link)
    {
        result = alloc_kernel_memory(sizeof(*result));
        
        result->low = empty_range.addr;
        result->high = empty_range.addr + size;
        result->parent_and_flags = (umm)empty_range.parent | VIRTUAL_MEMORY_COLOR_RED;
        *empty_range.link = result;
        fixup_virtual_memory_node_insertion(tree, result);
    }
    
    return result;
}

static void
remove_virtual_memory_node(VirtualMemoryTree *tree, VirtualMemoryNode *deleted)
{
    if(deleted->lhs && deleted->rhs)
    {
        VirtualMemoryNode *successor = deleted->rhs;
        while(successor->lhs)
            successor = successor->lhs;
        
        deleted->low = successor->low;
        deleted->high = successor->high;
        set_virtual_memory_node_flags_except_color(deleted, successor->parent_and_flags);
        deleted = successor;
    }
    
    if(deleted->lhs)
    {
        assert(!deleted->rhs &&
               virtual_memory_node_color(deleted) == VIRTUAL_MEMORY_COLOR_BLACK &&
               virtual_memory_node_color(deleted->lhs) == VIRTUAL_MEMORY_COLOR_RED);
        
        deleted->low = deleted->lhs->low;
        deleted->high = deleted->lhs->high;
        set_virtual_memory_node_flags_except_color(deleted, deleted->lhs->parent_and_flags);
        deleted = deleted->lhs;
    }
    else if(deleted->rhs)
    {
        assert(!deleted->lhs &&
               virtual_memory_node_color(deleted) == VIRTUAL_MEMORY_COLOR_BLACK &&
               virtual_memory_node_color(deleted->rhs) == VIRTUAL_MEMORY_COLOR_RED);
        
        deleted->low = deleted->rhs->low;
        deleted->high = deleted->rhs->high;
        set_virtual_memory_node_flags_except_color(deleted, deleted->rhs->parent_and_flags);
        deleted = deleted->rhs;
    }
    else if(virtual_memory_node_color(deleted) == VIRTUAL_MEMORY_COLOR_RED)
    {
        VirtualMemoryNode *parent = virtual_memory_node_parent(deleted);
        assert(parent && virtual_memory_node_color(parent) == VIRTUAL_MEMORY_COLOR_BLACK);
        do_nothing;
    }
    else // virtual_memory_node_color(deleted) == VIRTUAL_MEMORY_COLOR_BLACK
    {
        assert(virtual_memory_node_color(deleted) == VIRTUAL_MEMORY_COLOR_BLACK);
        
        VirtualMemoryNode *node = deleted;
        VirtualMemoryNode *parent = virtual_memory_node_parent(deleted);
        while(parent)
        {
            if(is_lhs_virtual_memory_node(parent, node))
            {
                // NOTE: since node is black, we must have a sibling
                VirtualMemoryNode *sibling = parent->rhs;
                assert(sibling);
                
                // NOTE: rotate the tree to make the sibling black
                if(virtual_memory_node_color(sibling) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    rotate_virtual_memory_tree_counterclockwise(tree, parent);
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_RED);
                    set_virtual_memory_node_color(sibling, VIRTUAL_MEMORY_COLOR_BLACK);
                    
                    // update sibling after rotation
                    sibling = parent->rhs;
                }
                
                assert(virtual_memory_node_color(sibling) == VIRTUAL_MEMORY_COLOR_BLACK);
                
                if(sibling->lhs &&
                   virtual_memory_node_color(sibling->lhs) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    rotate_virtual_memory_tree_clockwise(tree, sibling);
                    rotate_virtual_memory_tree_counterclockwise(tree, parent);
                    set_virtual_memory_node_color(sibling, virtual_memory_node_color(parent));
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
                    break;
                }
                else if(sibling->rhs &&
                        virtual_memory_node_color(sibling->rhs) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    rotate_virtual_memory_tree_counterclockwise(tree, parent);
                    set_virtual_memory_node_color(sibling, virtual_memory_node_color(parent));
                    set_virtual_memory_node_color(sibling->rhs, VIRTUAL_MEMORY_COLOR_BLACK);
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
                    break;
                }
                else if(virtual_memory_node_color(parent) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    set_virtual_memory_node_color(sibling, VIRTUAL_MEMORY_COLOR_RED);
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
                    break;
                }
                else // virtual_memory_node_color(parent) == VIRTUAL_MEMORY_COLOR_BLACK
                {
                    set_virtual_memory_node_color(sibling, VIRTUAL_MEMORY_COLOR_RED);
                    node = parent;
                }
            }
            else // is_rhs_virtual_memory_node(parent, node)
            {
                // NOTE: since node is black, we must have a sibling
                VirtualMemoryNode *sibling = parent->lhs;
                assert(sibling);
                
                // NOTE: rotate the tree to make the sibling black
                if(virtual_memory_node_color(sibling) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    rotate_virtual_memory_tree_clockwise(tree, parent);
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_RED);
                    set_virtual_memory_node_color(sibling, VIRTUAL_MEMORY_COLOR_BLACK);
                    
                    // update sibling after rotation
                    sibling = parent->lhs;
                }
                
                assert(virtual_memory_node_color(sibling) == VIRTUAL_MEMORY_COLOR_BLACK);
                
                if(sibling->rhs &&
                   virtual_memory_node_color(sibling->rhs) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    rotate_virtual_memory_tree_counterclockwise(tree, sibling);
                    rotate_virtual_memory_tree_clockwise(tree, parent);
                    set_virtual_memory_node_color(sibling, virtual_memory_node_color(parent));
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
                    break;
                }
                else if(sibling->lhs &&
                        virtual_memory_node_color(sibling->lhs) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    rotate_virtual_memory_tree_clockwise(tree, parent);
                    set_virtual_memory_node_color(sibling, virtual_memory_node_color(parent));
                    set_virtual_memory_node_color(sibling->lhs, VIRTUAL_MEMORY_COLOR_BLACK);
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
                    break;
                }
                else if(virtual_memory_node_color(parent) == VIRTUAL_MEMORY_COLOR_RED)
                {
                    set_virtual_memory_node_color(sibling, VIRTUAL_MEMORY_COLOR_RED);
                    set_virtual_memory_node_color(parent, VIRTUAL_MEMORY_COLOR_BLACK);
                    break;
                }
                else // virtual_memory_node_color(parent) == VIRTUAL_MEMORY_COLOR_BLACK
                {
                    set_virtual_memory_node_color(sibling, VIRTUAL_MEMORY_COLOR_RED);
                    node = parent;
                }
            }
            parent = virtual_memory_node_parent(node);
        }
    }
    
    VirtualMemoryNode *parent = virtual_memory_node_parent(deleted);
    if(!parent)
        tree->root = 0;
    else if(is_lhs_virtual_memory_node(parent, deleted))
        parent->lhs = 0;
    else // is_rhs_virtual_memory_node(parent, deleted)
        parent->rhs = 0;
    free_kernel_memory(deleted);
}

static void
init_virtual_memory_tree(VirtualMemoryTree *tree)
{
    clear_memory(tree, sizeof(*tree));
    tree->lowest = USER_SPACE_OFFSET;
    tree->highest = USER_SPACE_OFFSET + USER_SPACE_SIZE;
}

static u64 *
get_next_level_page_table(u64 entry)
{
    u64 *result = (u64 *)direct_mapped_virtual_address(entry & PAGE_ADDR_MASK);
    return result;
}

static u64 *
get_page_entry(u64 *page_table, void *virtual_addr)
{
    u32 l0_index = ((umm)virtual_addr >> 39) & 0x1ff;
    u32 l1_index = ((umm)virtual_addr >> 30) & 0x1ff;
    u32 l2_index = ((umm)virtual_addr >> 21) & 0x1ff;
    u32 l3_index = ((umm)virtual_addr >> 12) & 0x1ff;
    u32 offset = (umm)virtual_addr & 0xfff;
    
    u64 *l0_table = page_table;
    u64 *l1_table = get_next_level_page_table(l0_table[l0_index]);
    u64 *l2_table = get_next_level_page_table(l1_table[l1_index]);
    u64 *l3_table = get_next_level_page_table(l2_table[l2_index]);
    return &l3_table[l3_index];
}

static u32
increment_page_reference(u64 physical_addr)
{
    PagePool *page_pool = &g_kernel_state.page_pool;
    PageInfo *page_info = get_page_info(direct_mapped_virtual_address(physical_addr));
    
    return ++page_info->reference_count;
}

static u32
decrement_page_reference(u64 physical_addr)
{
    PagePool *page_pool = &g_kernel_state.page_pool;
    PageInfo *page_info = get_page_info(direct_mapped_virtual_address(physical_addr));
    {
        int x = 0;
    }
    assert(page_info->reference_count > 0);
    
    u32 reference_count = --page_info->reference_count;
    if(reference_count == 0)
        free_pages(direct_mapped_virtual_address(physical_addr));
    
    return reference_count;
}

static b32
is_l3_page_entry_iter_valid(L3PageEntryIter *iter)
{
    s32 l0_count = PAGE_TABLE_SIZE >> 3;
    return iter->l0_index != l0_count;
}

static void
advance_l3_page_entry_iter(L3PageEntryIter *iter)
{
    u64 *l0_table = iter->page_table;
    s32 l0_count = PAGE_TABLE_SIZE >> 3;
    s32 l1_count = PAGE_TABLE_SIZE >> 3;
    s32 l2_count = PAGE_TABLE_SIZE >> 3;
    s32 l3_count = PAGE_TABLE_SIZE >> 3;
    
    ++iter->l3_index;
    while(iter->l0_index < l0_count)
    {
        if(l0_table[iter->l0_index])
        {
            u64 *l1_table = get_next_level_page_table(l0_table[iter->l0_index]);
            while(iter->l1_index < l1_count)
            {
                if(l1_table[iter->l1_index])
                {
                    u64 *l2_table = get_next_level_page_table(l1_table[iter->l1_index]);
                    while(iter->l2_index < l2_count)
                    {
                        if(l2_table[iter->l2_index])
                        {
                            u64 *l3_table = get_next_level_page_table(l2_table[iter->l2_index]);
                            while(iter->l3_index < l3_count)
                            {
                                if(l3_table[iter->l3_index])
                                {
                                    iter->entry = &l3_table[iter->l3_index];
                                    return;
                                }
                                ++iter->l3_index;
                            }
                            iter->l3_index = 0;
                        }
                        ++iter->l2_index;
                    }
                    iter->l2_index = 0;
                }
                ++iter->l1_index;
            }
            iter->l1_index = 0;
        }
        ++iter->l0_index;
    }
}

static L3PageEntryIter
iterate_l3_page_entry(u64 *page_table)
{
    L3PageEntryIter iter = {0};
    
    iter.page_table = page_table;
    iter.l3_index = -1;
    advance_l3_page_entry_iter(&iter);
    return iter;
}

static void
duplicate_user_space(Process *to, Process *from)
{
    // copy page entries
    u64 *to_l0_table = (u64 *)alloc_pages(PAGE_TABLE_SIZE);
    u64 *from_l0_table = from->page_table;
    s32 l0_count = PAGE_TABLE_SIZE >> 3;
    s32 l1_count = PAGE_TABLE_SIZE >> 3;
    s32 l2_count = PAGE_TABLE_SIZE >> 3;
    s32 l3_count = PAGE_TABLE_SIZE >> 3;
    
    for(s32 l0_index = 0; l0_index < l0_count; ++l0_index)
    {
        if(!from_l0_table[l0_index])
            continue;
        
        u64 *to_l1_table = (u64 *)alloc_pages(PAGE_TABLE_SIZE);
        u64 *from_l1_table = get_next_level_page_table(from_l0_table[l0_index]);
        
        u64 l1_attrib = from_l0_table[l0_index] & ~PAGE_ADDR_MASK;
        to_l0_table[l0_index] = direct_mapped_physical_address(to_l1_table) | l1_attrib;
        
        for(s32 l1_index = 0; l1_index < l1_count; ++l1_index)
        {
            if(!from_l1_table[l1_index])
                continue;
            
            u64 *to_l2_table = (u64 *)alloc_pages(PAGE_TABLE_SIZE);
            u64 *from_l2_table = get_next_level_page_table(from_l1_table[l1_index]);
            
            u64 l2_attrib = from_l1_table[l1_index] & ~PAGE_ADDR_MASK;
            to_l1_table[l1_index] = direct_mapped_physical_address(to_l2_table) | l2_attrib;
            increment_page_reference(direct_mapped_physical_address(to_l1_table));
            
            for(s32 l2_index = 0; l2_index < l2_count; ++l2_index)
            {
                if(!from_l2_table[l2_index])
                    continue;
                
                u64 *to_l3_table = (u64 *)alloc_pages(PAGE_TABLE_SIZE);
                u64 *from_l3_table = get_next_level_page_table(from_l2_table[l2_index]);
                
                u64 l3_attrib = from_l2_table[l2_index] & ~PAGE_ADDR_MASK;
                to_l2_table[l2_index] = direct_mapped_physical_address(to_l3_table) | l3_attrib;
                increment_page_reference(direct_mapped_physical_address(to_l2_table));
                
                for(s32 l3_index = 0; l3_index < l3_count; ++l3_index)
                {
                    if(!from_l3_table[l3_index])
                        continue;
                    
                    to_l3_table[l3_index] = from_l3_table[l3_index];
                    increment_page_reference(direct_mapped_physical_address(to_l3_table));
                }
            }
        }
    }
    to->page_table = to_l0_table;
    
    // copy memory tree
    VirtualMemoryNode *from_node = from->memory_tree.root;
    if(from_node)
    {
        while(from_node->lhs)
            from_node = from_node->lhs;
    }
    
    while(from_node)
    {
        umm addr = from_node->low;
        umm size = from_node->high - from_node->low;
        VirtualMemoryNode *to_node = map_virtual_memory_node(&to->memory_tree, addr, size);
        assert(to_node);
        set_virtual_memory_node_flags_except_color(to_node, from_node->parent_and_flags);
        
        if(!(to_node->parent_and_flags & VIRTUAL_MEMORY_FLAG_MAPPED))
        {
            for(umm virtual_addr = to_node->low;
                virtual_addr < to_node->high;
                virtual_addr += PAGE_SIZE)
            {
                u64 *entry = get_page_entry(to_l0_table, (void *)virtual_addr);
                u64 physical_addr = *entry & PAGE_ADDR_MASK;
                increment_page_reference(physical_addr);
            }
        }
        
        from_node = next_virtual_memory_node(from_node);
    }
}

static u64 *
ensure_page_entry_exist(u64 *page_table, umm virtual_addr)
{
    u32 l0_index = (virtual_addr >> 39) & 0x1ff;
    u32 l1_index = (virtual_addr >> 30) & 0x1ff;
    u32 l2_index = (virtual_addr >> 21) & 0x1ff;
    u32 l3_index = (virtual_addr >> 12) & 0x1ff;
    u32 offset = virtual_addr & 0xfff;
    assert(offset == 0);
    
    u64 *l0_table = page_table;
    u64 *l1_table = 0;
    u64 *l2_table = 0;
    u64 *l3_table = 0;
    
    if(!l0_table[l0_index])
    {
        u64 *table = (u64 *)alloc_pages(PAGE_TABLE_SIZE);
        assert(table);
        
        l0_table[l0_index] = direct_mapped_physical_address(table) | PAGE_ATTRIB_TABLE;
    }
    l1_table = (u64 *)direct_mapped_virtual_address(l0_table[l0_index] & PAGE_ADDR_MASK);
    
    if(!l1_table[l1_index])
    {
        u64 *table = (u64 *)alloc_pages(PAGE_TABLE_SIZE);
        assert(table);
        
        l1_table[l1_index] = direct_mapped_physical_address(table) | PAGE_ATTRIB_TABLE;
        increment_page_reference(direct_mapped_physical_address(l1_table));
    }
    l2_table = (u64 *)direct_mapped_virtual_address(l1_table[l1_index] & PAGE_ADDR_MASK);
    
    if(!l2_table[l2_index])
    {
        u64 *table = (u64 *)alloc_pages(PAGE_TABLE_SIZE);
        assert(table);
        
        l2_table[l2_index] = direct_mapped_physical_address(table) | PAGE_ATTRIB_TABLE;
        increment_page_reference(direct_mapped_physical_address(l2_table));
    }
    
    l3_table = (u64 *)direct_mapped_virtual_address(l2_table[l2_index] & PAGE_ADDR_MASK);
    increment_page_reference(direct_mapped_physical_address(l3_table));
    
    return &l3_table[l3_index];
}

static void
decrement_page_entry_reference(u64 *page_table, umm virtual_addr)
{
    u32 l0_index = (virtual_addr >> 39) & 0x1ff;
    u32 l1_index = (virtual_addr >> 30) & 0x1ff;
    u32 l2_index = (virtual_addr >> 21) & 0x1ff;
    u32 l3_index = (virtual_addr >> 12) & 0x1ff;
    u32 offset = virtual_addr & 0xfff;
    assert(offset == 0);
    
    u64 *l0_table = page_table;
    u64 *l1_table = (u64 *)direct_mapped_virtual_address(l0_table[l0_index] & PAGE_ADDR_MASK);
    u64 *l2_table = (u64 *)direct_mapped_virtual_address(l1_table[l1_index] & PAGE_ADDR_MASK);
    u64 *l3_table = (u64 *)direct_mapped_virtual_address(l2_table[l2_index] & PAGE_ADDR_MASK);
    
    l3_table[l3_index] = 0;
    if(decrement_page_reference(direct_mapped_physical_address(l3_table)) == 0)
    {
        l2_table[l2_index] = 0;
        if(decrement_page_reference(direct_mapped_physical_address(l2_table)) == 0)
        {
            l1_table[l1_index] = 0;
            if(decrement_page_reference(direct_mapped_physical_address(l1_table)) == 0)
            {
                l0_table[l0_index] = 0;
            }
        }
    }
}

static void
map_page(u64 *page_table, VirtualMemoryNode *node, umm virtual_addr, u64 physical_addr)
{
    if(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_READ)
    {
        u64 *entry = ensure_page_entry_exist(page_table, virtual_addr);
        u64 attrib = PAGE_ATTRIB_ACCESS | PAGE_ATTRIB_L3_PAGE | PAGE_ATTRIB_PXN;
        if(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_WRITE)
            attrib |= PAGE_ATTRIB_RW_EL0;
        else
            attrib |= PAGE_ATTRIB_RO_EL0;
        
        if(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_DEVICE)
            attrib |= PAGE_ATTRIB_MAIR_INDEX_DEVICE;
        else
            attrib |= PAGE_ATTRIB_MAIR_INDEX_NORMAL;
        
        if(!(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_EXECUTE))
            attrib |= PAGE_ATTRIB_UXN;
        
        if(!physical_addr)
        {
            void *page = alloc_pages(PAGE_SIZE);
            physical_addr = direct_mapped_physical_address(page);
        }
        
        if(!(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_DEVICE))
            increment_page_reference(physical_addr);
        
        *entry = physical_addr | attrib;
    }
}

static void *
alloc_user_memory(Process *process, void *addr_hint, umm size,
                  AllocationType type, MemoryPermission permission)
{
    void *result = 0;
    if(size > 0)
    {
        size = align_up(size, PAGE_SIZE);
        
        VirtualMemoryTree *tree = &process->memory_tree;
        VirtualMemoryNode *node = 0;
        
        if(addr_hint)
            node = map_virtual_memory_node(tree, (umm)addr_hint, size);
        
        if(!node)
            node = insert_virtual_memory_node(tree, size);
        
        if(node)
        {
            result = (void *)node->low;
            
            if(permission & MemoryPermission_read)
                node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_READ;
            if(permission & MemoryPermission_write)
                node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_WRITE;
            if(permission & MemoryPermission_execute)
                node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_EXECUTE;
            
            if(type == AllocationType_commit)
            {
                for(umm virtual_addr = node->low;
                    virtual_addr < node->high;
                    virtual_addr += PAGE_SIZE)
                {
                    map_page(process->page_table, node, virtual_addr, 0);
                }
                invalidate_entire_tlb();
            }
        }
    }
    return result;
}

static void *
map_user_memory(Process *process, u64 physical_addr, umm size,
                AllocationType type, MemoryPermission permission)
{
    assert(type == AllocationType_commit);
    
    void *result = 0;
    if(size > 0)
    {
        size = align_up(size, PAGE_SIZE);
        VirtualMemoryNode *node = insert_virtual_memory_node(&process->memory_tree, size);
        if(node)
        {
            result = (void *)node->low;
            
            node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_MAPPED;
            if(permission & MemoryPermission_read)
                node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_READ;
            if(permission & MemoryPermission_write)
                node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_WRITE;
            if(permission & MemoryPermission_execute)
                node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_EXECUTE;
            
            for(umm virtual_addr = node->low; virtual_addr < node->high; virtual_addr += PAGE_SIZE)
            {
                map_page(process->page_table, node, virtual_addr, physical_addr);
                physical_addr += PAGE_SIZE;
            }
            invalidate_entire_tlb();
        }
    }
    return result;
}

static void *
map_device_memory(Process *process, umm virtual_addr, u64 physical_addr, umm size)
{
    void *result = 0;
    VirtualMemoryTree *tree = &process->memory_tree;
    if(size > 0)
    {
        size = align_up(size, PAGE_SIZE);
        VirtualMemoryNode *node = map_virtual_memory_node(tree, virtual_addr, size);
        if(node)
        {
            assert(virtual_addr == node->low);
            result = (void *)node->low;
            
            node->parent_and_flags |= VIRTUAL_MEMORY_FLAG_MAPPED | VIRTUAL_MEMORY_FLAG_DEVICE |
                                      VIRTUAL_MEMORY_FLAG_READ | VIRTUAL_MEMORY_FLAG_WRITE;
            
            while(virtual_addr < node->high)
            {
                map_page(process->page_table, node, virtual_addr, physical_addr);
                virtual_addr += PAGE_SIZE;
                physical_addr += PAGE_SIZE;
            }
            invalidate_entire_tlb();
        }
    }
    return result;
}

static b32
free_user_memory(Process *process, void *user_addr)
{
    b32 result = 0;
    VirtualMemoryTree *tree = &process->memory_tree;
    VirtualMemoryNode *node = find_virtual_memory_node(tree, user_addr);
    if(node)
    {
        result = 1;
        for(umm virtual_addr = node->low; virtual_addr < node->high; virtual_addr += PAGE_SIZE)
        {
            u64 *entry = get_page_entry(process->page_table, (void *)virtual_addr);
            u64 physical_addr = *entry & PAGE_ADDR_MASK;
            
            if(!(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_MAPPED))
                decrement_page_reference(physical_addr);
        }
        
        remove_virtual_memory_node(tree, node);
    }
    return result;
    
}

static u64 *
get_current_user_page_table(void)
{
    u64 *result = (u64 *)read_ttbr0_el1();
    return result;
}

static void
set_current_user_page_table(u64 *page_table)
{
    write_ttbr0_el1((u64)direct_mapped_physical_address(page_table));
    invalidate_entire_tlb();
}

// TODO: copy_to_user() can be called frequently, instead of invalidate tlb every time we should use
// begin/end
static void
copy_to_user(Process *process, void *to, void *from, umm size)
{
    u64 *prev_page_table = get_current_user_page_table();
    set_current_user_page_table(process->page_table);
    copy_memory(to, from, size);
    set_current_user_page_table(prev_page_table);
}

// FIXME: we assume the page fault is 1 byte only?
static void
handle_page_fault(Process *process, void *fault_addr)
{
    VirtualMemoryTree *tree = &process->memory_tree;
    VirtualMemoryNode *node = find_virtual_memory_node(tree, fault_addr);
    if(node)
    {
        map_page(process->page_table, node, (umm)fault_addr, 0);
        invalidate_entire_tlb();
        
        print_string("[Translation fault]: ");
        print_hex64((umm)fault_addr);
        print_string("\r\n");
    }
    else
    {
        print_string("[Segmentation fault]: ");
        print_hex64((umm)fault_addr);
        print_string("\r\n");
        exit_process(process, 1);
        yield_physical_thread();
    }
}

// FIXME: we assume the page fault is 1 byte only?
static void
handle_copy_on_write(Process *process, void *fault_addr)
{
    VirtualMemoryTree *tree = &process->memory_tree;
    VirtualMemoryNode *node = find_virtual_memory_node(tree, fault_addr);
    if(node && (node->parent_and_flags & (VIRTUAL_MEMORY_FLAG_READ | VIRTUAL_MEMORY_FLAG_WRITE)))
    {
        u64 *entry = get_page_entry(process->page_table, fault_addr);
        u64 attrib = (*entry & ~(PAGE_ADDR_MASK | PAGE_ATTRIB_RW_MASK)) | PAGE_ATTRIB_RW_EL0;
        
        if(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_MAPPED)
        {
            u64 physical_addr = *entry & PAGE_ADDR_MASK;
            *entry = physical_addr | attrib;
        }
        else
        {
            u64 old_entry = *entry;
            umm virtual_addr = align_down((umm)fault_addr, PAGE_SIZE);
            map_page(process->page_table, node, virtual_addr, 0);
            invalidate_entire_tlb();
            
            void *old_page = direct_mapped_virtual_address(old_entry & PAGE_ADDR_MASK);
            void *new_page = direct_mapped_virtual_address(*entry & PAGE_ADDR_MASK);
            copy_memory(new_page, old_page, PAGE_SIZE);
            
            if(!(node->parent_and_flags & VIRTUAL_MEMORY_FLAG_MAPPED))
                decrement_page_reference(direct_mapped_physical_address(old_page));
        }
    }
    else
    {
        print_string("[Segmentation fault]: ");
        print_hex64((umm)fault_addr);
        print_string("\r\n");
        exit_process(process, 1);
        yield_physical_thread();
    }
}
