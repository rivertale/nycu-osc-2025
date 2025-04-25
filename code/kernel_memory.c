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

static void
reserve_memory_region(MemoryRegionList *list, void *low, void *high)
{
    assert(low <= high);

    u32 cur = 0;
    for(u32 index = 0; index < list->count; ++index)
    {
        MemoryRegion *region = list->regions + index;
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
                // NOTE: must not overlap with other regions, split and terminate
                assert(list->count < MAX_MEMORY_REGION_COUNT);
                for(u32 i = list->count - 1; i > index; --i)
                    list->regions[i + 1] = list->regions[i];

                void *low0 = region[0].low;
                void *high0 = low;
                void *low1 = high;
                void *high1 = region[0].high;
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
init_memory_region_list(MemoryRegionList *list, void *low, void *high)
{
    assert(low <= high);

    list->count = 0;

    assert(list->count < MAX_MEMORY_REGION_COUNT);
    MemoryRegion *region = list->regions + list->count++;
    region->low = low;
    region->high = high;
}

static PageInfo *
get_page_info(PagePool *pool, void *page)
{
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

    u64 mask = (1ull << (pool->next_exponent_after_page_count + 1)) - 1;
    u64 block_index = ((u8 *)block - pool->base) >> (PAGE_MIN_ALLOC_EXPONENT + order);
    u64 order_bits = (-1ull << (pool->next_exponent_after_page_count - order + 1)) & mask;

    assert((order_bits & block_index) == 0);

    u64 reservation_count = next_power_of_two(pool->page_count) << 1;
    if((order_bits | block_index) >= reservation_count)
    {
        int x = 1;
    }

    assert((order_bits | block_index) < reservation_count);
    return order_bits | block_index;
}

static b32
page_pool_is_reserved(PagePool *pool, s32 order, void *block)
{
    u64 bit_index = page_pool_get_reservation_bit_index(pool, order, block);
    return pool->reservation[bit_index >> 6] & (1ull << (bit_index & 63));
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
page_pool_calculate_maximum_order(void *ptr)
{
    s32 result = find_least_significant_bit((u64)ptr) - PAGE_MIN_ALLOC_EXPONENT;
    if(result > PAGE_MAX_ALLOC_ORDER)
        result = PAGE_MAX_ALLOC_ORDER;

    return result;
}

static void
free_pages(PagePool *pool, void *ptr)
{
    if(!ptr)
        return;

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
alloc_pages(PagePool *pool, umm size)
{
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

            PageBlock *b = (PageBlock *)result;
            b->prev->next = b->next;
            b->next->prev = b->prev;

            // double_link_remove((PageBlock *)result);
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
init_page_pool(PagePool *pool, MemoryRegionList *region_list)
{
    clear_memory(pool, sizeof(*pool));
    for(s32 order = 0; order <= PAGE_MAX_ALLOC_ORDER; ++order)
    {
        double_link_init(&pool->free_block[order]);
    }

    if(region_list->count == 0)
        return;

    MemoryRegion *first_region = region_list->regions;
    MemoryRegion *last_region = region_list->regions + region_list->count - 1;
    u64 reservation_lowest = align_down((u64)first_region->low, PAGE_MAX_ALLOC_SIZE);
    u64 reservation_highest = align_up((u64)last_region->high, PAGE_MAX_ALLOC_SIZE);

    u64 page_count = (reservation_highest - reservation_lowest) >> PAGE_MIN_ALLOC_EXPONENT;
    u64 reservation_count = next_power_of_two(page_count) << 1;

    u64 page_info_size = page_count * sizeof(PageInfo);
    u64 reservation_size = align_up(reservation_count, 64) >> 3;

    pool->next_exponent_after_page_count = find_most_significant_bit(next_power_of_two(page_count));
    pool->page_count = page_count;
    pool->base = (u8 *)reservation_lowest;

    // NOTE: reserve byte 0
    if(first_region->low == 0)
        first_region->low = (void *)1;

    // NOTE: reserve memory for page info array
    for(s32 index = 0; index < region_list->count; ++index)
    {
        MemoryRegion *region = region_list->regions + index;
        if((u64)region->high - (u64)region->low >= page_info_size)
        {
            pool->page_infos = (PageInfo *)region->low;
            region->low = (u8 *)region->low + page_info_size;
            break;
        }
    }
    clear_memory(pool->page_infos, page_info_size);

    // NOTE: reserve memory for reservation bit array
    for(s32 index = 0; index < region_list->count; ++index)
    {
        MemoryRegion *region = region_list->regions + index;
        if((u64)region->high - (u64)region->low >= reservation_size)
        {
            pool->reservation = (u64 *)region->low;
            region->low = (u8 *)region->low + reservation_size;
            break;
        }
    }
    clear_memory(pool->reservation, reservation_size);

    for(s32 index = 0; index < region_list->count; ++index)
    {
        MemoryRegion *region = region_list->regions + index;

        u8 *low = (u8 *)align_up((u64)region->low, PAGE_MIN_ALLOC_SIZE);
        u8 *high = (u8 *)align_down((u64)region->high, PAGE_MIN_ALLOC_SIZE);
        s32 low_order = page_pool_calculate_maximum_order(low);
        s32 high_order = page_pool_calculate_maximum_order(high);

        while(low < high)
        {
            assert(0 <= low_order && low_order <= PAGE_MAX_ALLOC_ORDER);
            assert(0 <= high_order && high_order <= PAGE_MAX_ALLOC_ORDER);

            if(low_order <= high_order)
            {
                u32 block_size = 1 << (low_order + PAGE_MIN_ALLOC_EXPONENT);

                PageBlock *block = (PageBlock *)low;
                double_link_insert_at_last(&pool->free_block[low_order], block);

                low += block_size;
                low_order = page_pool_calculate_maximum_order(low);
            }
            else
            {
                u32 block_size = 1 << (high_order + PAGE_MIN_ALLOC_EXPONENT);

                PageBlock *block = (PageBlock *)(high - block_size);
                double_link_insert_at_last(&pool->free_block[high_order], block);

                high -= block_size;
                high_order = page_pool_calculate_maximum_order(high);
            }
        }
    }

    for(u8 *block = (u8 *)reservation_lowest;
        block < (u8 *)reservation_highest;
        block += PAGE_MAX_ALLOC_SIZE)
    {
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
                block = (PageBlock *)((u64)block & ~block_size);
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

    void *region_list_ptr = (void *)align_down((u64)region_list, PAGE_MIN_ALLOC_SIZE);
    free_pages(pool, region_list_ptr);
}

static Slab *
create_slab(MemoryAllocator *allocator, s32 cache_index, umm allocation_size)
{
    PagePool *page_pool = allocator->page_pool;
    if(!allocator->first_free_slab)
    {
        // TODO: handle error
        u8 *memory = alloc_pages(page_pool, PAGE_SIZE);
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

    // TODO: handle error
    slab->memory = alloc_pages(page_pool, PAGE_SIZE);
    slab->cache_index = cache_index;
    slab->allocation_count = 0;
    slab->max_allocation_count = PAGE_SIZE / allocation_size;
    assert(slab->memory);

    PageInfo *page_info = get_page_info(page_pool, slab->memory);
    page_info->slab = slab;

    for(um32 offset = 0; offset + allocation_size <= PAGE_SIZE; offset += allocation_size)
    {
        SlabAllocation *allocation = (SlabAllocation *)(slab->memory + offset);
        allocation->next_free = slab->first_free_allocation;
        slab->first_free_allocation = allocation;
    }

    double_link_insert_at_last(&allocator->partial_cache[cache_index], &slab->link);
    return slab;
}

static void
free_memory(MemoryAllocator *allocator, void *ptr)
{
    if(!ptr)
        return;

    PagePool *page_pool = allocator->page_pool;
    u8 *page_boundary = (u8 *)align_down((u64)ptr, PAGE_SIZE);
    PageInfo *page_info = get_page_info(page_pool, page_boundary);

    Slab *slab = page_info->slab;
    if(slab)
    {
        assert(slab->allocation_count > 0);

        --slab->allocation_count;
        if(slab->allocation_count == 0)
        {
            page_info->slab = 0;
            free_pages(page_pool, slab->memory);

            double_link_remove(&slab->link);
            slab->next_free = allocator->first_free_slab;
            allocator->first_free_slab = slab;
        }
        else
        {
            if(slab->allocation_count == slab->max_allocation_count - 1)
            {
                double_link_remove(&slab->link);
                double_link_insert_at_last(&allocator->partial_cache[slab->cache_index], &slab->link);
            }
            SlabAllocation *allocation = (SlabAllocation *)ptr;
            allocation->next_free = slab->first_free_allocation;
            slab->first_free_allocation = allocation;
        }
    }
    else
    {
        free_pages(page_pool, page_boundary);
    }
}

static void *
alloc_memory(MemoryAllocator *allocator, umm size)
{
    void *result = 0;
    if(size > 0)
    {
        if(size <= ALLOCATOR_MAX_SLAB_SIZE)
        {
            s32 cache_index = 0;
            if(size <= ALLOCATOR_MAX_MAPPED_SIZE)
            {
                size = align_up(size, ALLOCATOR_MAP_GRANULARITY);

                cache_index =
                    g_allocator_size_to_cache_index_map[size >> ALLOCATOR_MAP_GRANULARITY_EXPONENT];
                size = g_allocator_cache_index_to_size_map[cache_index];
            }
            else
            {
                size = next_power_of_two(size);

                cache_index = find_most_significant_bit(size);
                size = 1 << cache_index;
            }

            if(double_link_is_empty(&allocator->partial_cache[cache_index]))
                create_slab(allocator, cache_index, size);

            Slab *slab = (Slab *)allocator->partial_cache[cache_index].next;
            assert(slab);

            result = (void *)slab->first_free_allocation;
            slab->first_free_allocation = ((SlabAllocation *)result)->next_free;
            assert(result);

            ++slab->allocation_count;
            if(slab->allocation_count == slab->max_allocation_count)
            {
                double_link_remove(&slab->link);
                double_link_insert_at_last(&allocator->full_cache[cache_index], &slab->link);
            }

            clear_memory(result, size);
        }
        else
        {
            result = alloc_pages(allocator->page_pool, size);
        }
    }

    return result;
}

static void
init_memory_allocator(MemoryAllocator *allocator, PagePool *page_pool)
{
    clear_memory(allocator, sizeof(*allocator));
    allocator->page_pool = page_pool;
    for(s32 index = 0; index <= ALLOCATOR_MAX_SLAB_EXPONENT; ++index)
    {
        double_link_init(&allocator->full_cache[index]);
        double_link_init(&allocator->partial_cache[index]);
    }
}
