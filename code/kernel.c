#include "kernel.h"
#include "devicetree.c"
#include "watchdog.c"
#include "uart.c"
#include "mailbox.c"
#include "cpio.c"

static void
print_buffer(c8 *buffer, umm size)
{
    mini_uart_write(buffer, size);
}

static void
print_string(c8 *message)
{
    umm len = string_len(message);
    mini_uart_write(message, len);
}

static void
print_hex32(u32 value)
{
    static c8 hex_digit[16] =
    {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    
    c8 digits[] =
    {
        '0', 'x',
        hex_digit[(value >> 28) & 0xf],
        hex_digit[(value >> 24) & 0xf],
        hex_digit[(value >> 20) & 0xf],
        hex_digit[(value >> 16) & 0xf],
        hex_digit[(value >> 12) & 0xf],
        hex_digit[(value >> 8) & 0xf],
        hex_digit[(value >> 4) & 0xf],
        hex_digit[(value >> 0) & 0xf],
        '\0'
    };
    print_string(digits);
}

static void
print_hex64(u64 value)
{
    static c8 hex_digit[16] =
    {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    
    c8 digits[] =
    {
        '0', 'x',
        hex_digit[(value >> 60) & 0xf],
        hex_digit[(value >> 56) & 0xf],
        hex_digit[(value >> 52) & 0xf],
        hex_digit[(value >> 48) & 0xf],
        hex_digit[(value >> 44) & 0xf],
        hex_digit[(value >> 40) & 0xf],
        hex_digit[(value >> 36) & 0xf],
        hex_digit[(value >> 32) & 0xf],
        hex_digit[(value >> 28) & 0xf],
        hex_digit[(value >> 24) & 0xf],
        hex_digit[(value >> 20) & 0xf],
        hex_digit[(value >> 16) & 0xf],
        hex_digit[(value >> 12) & 0xf],
        hex_digit[(value >> 8) & 0xf],
        hex_digit[(value >> 4) & 0xf],
        hex_digit[(value >> 0) & 0xf],
        '\0'
    };
    print_string(digits);
}

static void
print_u64(u64 value)
{
    c8 digits[32];
    
    um32 cur = array_count(digits);
    if(value > 0)
    {
        while(value > 0)
        {
            digits[--cur] = value % 10 + '0';
            value /= 10;
        }
    }
    else
    {
        digits[--cur] = '0';
    }
    
    print_string(digits + cur);
}

static void
scan_line(c8 *string, um32 max_len)
{
    if(max_len > 0)
    {
        c8 *cur = string;
        umm remaining_len = max_len;
        while(remaining_len > 1)
        {
            c8 c = mini_uart_read_byte();
            if(c == 0x8 || c == 0x7f) // NOTE: backspace generates a DEL on QEMU
            {
                if(cur > string)
                {
                    mini_uart_write_const("\x08 \x08");
                    --cur;
                    ++remaining_len;
                }
            }
            else
            {
                mini_uart_write_byte(c);
                *cur++ = c;
                --remaining_len;
            }
            
            if(c == '\r')
                break;
        }
        *cur++ = '\0';
    }
    mini_uart_write_byte('\n');
}

static
DEVICETREE_CALLBACK(match_and_init_cpio)
{
    b32 result = 0;
    if(string_match(path, "/chosen/") &&
       string_match(prop_name, "linux,initrd-start"))
    {
        result = 1;
        void **cpio_addr = (void **)userdata;
        *cpio_addr = (void *)(umm)devicetree_u32(prop);
    }
    return result;
}

static
DEVICETREE_CALLBACK(print_devicetree)
{
    print_string(path);
    print_string(" - ");
    print_string(prop_name);
    print_string("\r\n");
    print_buffer((c8 *)prop, prop_size);
    print_string(" { ");
    
    for(um32 index = 0; index < prop_size; ++index)
    {
        if(index != 0)
            print_string(", ");
        
        c8 hex_digits[16] =
        {
            '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
        };
        
        c8 byte[3] =
        {
            hex_digits[(prop[index] >> 4) & 15],
            hex_digits[(prop[index] >> 0) & 15],
            '\0'
        };
        print_string(byte);
    }
    print_string(" }\r\n");
    return 0;
}

static s32
count_leading_zeros(u64 value)
{
    s64 result = 0;
    __asm__ volatile ("clz %0, %1" : "=r"(result) : "r"(value));
    return result;
}

static s32
get_most_significant_bit_position(u64 value)
{
    return 63 - count_leading_zeros(value);
}

static s32
align_to_power_of_two(u64 value)
{
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return value + 1;
}

static u64
heap_get_served_bit_index(Heap *heap, s32 order, void *block)
{
    u64 block_index = ((u8 *)block - heap->base) >> order;
    s32 order_shift = heap->order_shift - (order - HEAP_MIN_ORDER);
    u64 bit_index = ((-1ull << order_shift) | block_index) & heap->served_mask;
    return bit_index;
}

static void
heap_mark_served(Heap *heap, s32 order, void *block)
{
    u64 bit_index = heap_get_served_bit_index(heap, order, block);
    heap->served[bit_index >> 6] |= (1ull << (bit_index & 63));
}

static void
heap_unmark_served(Heap *heap, s32 order, void *block)
{
    u64 bit_index = heap_get_served_bit_index(heap, order, block);
    heap->served[bit_index >> 6] &= ~(1ull << (bit_index & 63));
}

static b32
heap_is_served(Heap *heap, s32 order, void *block)
{
    u64 bit_index = heap_get_served_bit_index(heap, order, block);
    return heap->served[bit_index >> 6] & (1ull << (bit_index & 63));
}

static void *
heap_alloc(Heap *heap, umm size)
{
    void *result = 0;
    if(size > 0)
    {
        if(size < HEAP_MIN_ALLOCATION)
            size = HEAP_MIN_ALLOCATION;
        size = next_power_of_two(size);
        
        s32 alloc_order = get_most_significant_bit_position(size);
        assert(HEAP_MIN_ORDER <= alloc_order && alloc_order <= HEAP_MAX_ORDER);
        heap->in_used += (1 << alloc_order);
        
        s32 order = alloc_order;
        while(order <= HEAP_MAX_ORDER)
        {
            if(!double_link_is_empty(&heap->free_block[order]))
                break;
            ++order;
        }
        
        if(order > HEAP_MAX_ORDER)
        {
            // NOTE: out of memory
            assert(0);
        }
        
        while(order > alloc_order)
        {
            HeapBlock *block = heap->free_block[order].next;
            double_link_remove(block);
            heap_mark_served(heap, order, block);
            
            // NOTE: split the block until it fits the allocated size
            --order;
            um32 split_size = 1 << order;
            HeapBlock *block0 = (HeapBlock *)((u8 *)block);
            HeapBlock *block1 = (HeapBlock *)((u8 *)block + split_size);
            
            // NOTE: the block at lower address will be allocated first, this is crucial for 
            // bootstrap bitmap allocation in heap_init()
            double_link_insert_at_last(&heap->free_block[order], block0);
            double_link_insert_at_last(&heap->free_block[order], block1);
        }
        
        result = (void *)heap->free_block[alloc_order].next;
        double_link_remove((HeapBlock *)result);
        heap_mark_served(heap, order, result);
    }
    return result;
}

static void
heap_free(Heap *heap, void *ptr)
{
    HeapBlock *block = (HeapBlock *)ptr;
    s32 order = HEAP_MIN_ORDER;
    while(order <= HEAP_MAX_ORDER)
    {
        if(heap_is_served(heap, order, block))
            break;
        
        ++order;
    }
    assert(order <= HEAP_MAX_ORDER);
    heap_unmark_served(heap, order, block);
    heap->in_used -= (1 << order);
    
    while(order < HEAP_MAX_ORDER)
    {
        umm offset = (u8 *)block - heap->base;
        HeapBlock *buddy = (HeapBlock *)(heap->base + (offset ^ (1 << order)));
        if(heap_is_served(heap, order, buddy))
            break;
        
        heap_unmark_served(heap, order, block);
        double_link_remove(buddy);
        
        block = (HeapBlock *)(heap->base + (offset & ~(1 << order)));
        ++order;
    }
    double_link_insert_at_last(&heap->free_block[order], block);
}

static void
heap_init(Heap *heap, void *addr, umm size)
{
    size = align2_up(size - ((umm)addr & 15), HEAP_MAX_ALLOCATION);
    addr = (void *)align2_up((umm)addr, 16);
    
    // NOTE: addr must align to 16 byte, and size must be a power of two
    
    clear_memory(heap, sizeof(*heap));
    u64 max_block_count = size / HEAP_MIN_ALLOCATION;
    u64 max_served_count = max_block_count << 1;
    u64 served_size = align2_up(max_served_count, 8 * HEAP_MIN_ALLOCATION) / 8;
    
    heap->total = size;
    heap->served_mask = max_served_count - 1;
    heap->order_shift = get_most_significant_bit_position(max_block_count) + 1;
    heap->base = addr;
    heap->served = (u64 *)addr;
    
    clear_memory(heap->served, served_size);
    for(s32 order = HEAP_MIN_ORDER; order <= HEAP_MAX_ORDER; ++order)
        double_link_init(&heap->free_block[order]);
    
    s32 order = get_most_significant_bit_position(served_size);
    umm block_size = 1 << order;
    u8 *cur = (u8 *)addr + block_size;
    umm remaining_size = size - block_size;
    while(order < HEAP_MAX_ORDER)
    {
        block_size = 1 << order;
        if(remaining_size < block_size)
            break;
        double_link_insert_at_last(&heap->free_block[order], (HeapBlock *)cur);
        heap_mark_served(heap, order, cur);
        cur += block_size;
        remaining_size -= block_size;
        ++order;
    }
    
    while(remaining_size >= HEAP_MAX_ALLOCATION)
    {
        double_link_insert_at_last(&heap->free_block[HEAP_MAX_ORDER], (HeapBlock *)cur);
        heap_mark_served(heap, HEAP_MAX_ORDER, cur);
        cur += HEAP_MAX_ALLOCATION;
        remaining_size -= HEAP_MAX_ALLOCATION;
    }
    
    assert(remaining_size == 0);
}

static u64
parse_u64(c8 *buffer)
{
    u64 result = 0;
    for(c8 *c = buffer; *c; ++c)
        result = result * 10 + (*c - '0');
    
    return result;
}

static u64
parse_hex64(c8 *buffer)
{
    u64 result = 0;
    
    if(buffer[0] == '0' && buffer[1] == 'x')
        buffer += 2;
    
    for(c8 *c = buffer; *c; ++c)
    {
        if('0' <= *c && *c <= '9')
            result = (result << 4) + (*c - '0');
        else if('A' <= *c && *c <= 'F')
            result = (result << 4) + (*c - 'A' + 10);
        else if('a' <= *c && *c <= 'f')
            result = (result << 4) + (*c - 'a' + 10);
    }
    return result;
}

static c8 *
tokenize(c8 *buffer, um32 *o_count)
{
    um32 count = 0;
    c8 *c = buffer;
    while(*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n')
        *c++ = '\0';
    
    c8 *first_token = c;
    c8 prev_c = '\0';
    while(*c)
    {
        if(prev_c == '\0')
            ++count;
        
        if(*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n')
            *c = '\0';
        prev_c = *c++;
    }
    
    if(o_count)
        *o_count = count;
    return first_token;
}

static c8 *
next_token(c8 *token)
{
    c8 *result = token + string_len(token);
    while(*result == '\0')
        ++result;
    return result;
}

void
kernel_main(void *devicetree_addr)
{
    mini_uart_init();
    
    Heap heap;
    heap_init(&heap, &heap_begin, (umm)&heap_size);
    
    // devicetree_traverse(devicetree_addr, print_devicetree, 0);
    devicetree_traverse(devicetree_addr, match_and_init_cpio, &g_cpio_base);
    
    print_string("Hello, Sailor!\r\n");
    for(;;)
    {
        c8 command[256];
        print_string("# ");
        scan_line(command, array_count(command));
        
        um32 token_count = 0;
        c8 *token = tokenize(command, &token_count);
        
        if(token_count == 0)
        {
            do_nothing;
        }
        else if(string_match(token, "help"))
        {
            c8 *usage =
                "help    : print this help menu\r\n"
                "hello   : print Hello World!\r\n"
                "mailbox : print hardware's information\r\n"
                "ls      : list files\r\n"
                "cat     : print file\r\n"
                "reboot  : reboot\r\n"
                "alloc   : allocate heap memory\r\n"
                "free    : free heap memory\r\n";
            print_string(usage);
        }
        else if(string_match(token, "hello"))
        {
            print_string("Hello World!\r\n");
        }
        else if(string_match(token, "mailbox"))
        {
            u32 revision = query_board_revision();
            ArmMemoryInfo memory_info = query_arm_memory_info();
            print_string("Mailbox info:\r\n");
            print_string("Board rivision: ");
            print_hex32(revision);
            print_string("\r\n");
            print_string("ARM memory base address: ");
            print_hex32(memory_info.base);
            print_string("\r\n");
            print_string("ARM memory size: ");
            print_hex32(memory_info.size);
            print_string("\r\n");
        }
        else if(string_match(token, "ls"))
        {
            for(void *handle = cpio_find_first_file();
                handle;
                handle = cpio_next_file(handle))
            {
                um32 len;
                c8 *file_name = cpio_get_file_name(handle, &len);
                print_buffer(file_name, len);
                print_string("\r\n");
            }
        }
        else if(string_match(token, "cat"))
        {
            if(token_count == 2)
            {
                c8 *file_name = next_token(token);
                void *handle = cpio_find_file(file_name);
                if(handle)
                {
                    um32 len;
                    c8 *file_content = (c8 *)cpio_get_file_content(handle, &len);
                    print_buffer(file_content, len);
                    print_string("\r\n");
                }
                else
                {
                    print_string("File not found: ");
                    print_string(file_name);
                    print_string("\r\n");
                }
            }
            else
            {
                c8 *usage = "Usage: cat <file>\r\n";
                print_string(usage);
            }
        }
        else if(string_match(token, "alloc"))
        {
            if(token_count == 2)
            {
                umm size = parse_u64(next_token(token));
                void *ptr = heap_alloc(&heap, size);
                print_hex64((umm)ptr);
                print_string("\r\n");
                print_string("Total ");
                print_u64(heap.in_used);
                print_string(" byte allocated\r\n");
            }
            else
            {
                c8 *usage = "Usage: alloc <size>\r\n";
                print_string(usage);
            }
        }
        else if(string_match(token, "free"))
        {
            if(token_count == 2)
            {
                void *ptr = (void *)parse_hex64(next_token(token));
                heap_free(&heap, ptr);
                print_string("Total ");
                print_u64(heap.in_used);
                print_string(" byte left\r\n");
                
            }
        }
        else if(string_match(token, "reboot"))
        {
            print_string("Rebooting...\r\n");
            watchdog_reboot(1000);
        }
        else
        {
            print_string("Unrecognized command: ");
            print_string(token);
            print_string("\r\n");
        }
    }
}