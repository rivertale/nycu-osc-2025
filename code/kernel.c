#include "kernel.h"
#include "uart.c"
static void print_buffer(c8 *buffer, umm size);
static void print_string(c8 *message);
static void print_hex32(u32 value);
static void print_hex64(u64 value);
static void print_u64(u64 value);
#include "kernel_bridge.c"
#include "kernel_timer.c"
#include "kernel_memory.c"
#include "kernel_scheduler.c"
#include "kernel_device.c"
#include "kernel_process.c"
#include "kernel_signal.c"
#include "kernel_syscall.c"
#include "kernel_interrupt.c"

static void
print_buffer(c8 *buffer, umm size)
{
    write_console(buffer, size);
}

static void
print_char(c8 c)
{
    write_console(&c, 1);
}

static void
print_string(c8 *message)
{
    umm len = string_len(message);
    write_console(message, len);
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
            c8 c;
            read_console(&c, sizeof(c));
            if(c == 0x8 || c == 0x7f) // NOTE: backspace generates a DEL on QEMU
            {
                if(cur > string)
                {
                    mini_uart_write_const("\x08 \x08");
                    --cur;
                    ++remaining_len;
                }
            }
            else if(c == 0x1b) // NOTE: echo and skip escape sequence
            {
                read_console(&c, sizeof(c));
                mini_uart_write_byte(0x1b);
                mini_uart_write_byte('[');
                if(c == 'O' || c == '[')
                {
                    for(;;)
                    {
                        read_console(&c, sizeof(c));
                        mini_uart_write_byte(c);
                        if(0x40 <= c && c <= 0x7e)
                            break;
                    }
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
TIMER_CALLBACK(timer_print_string)
{
    c8 *task_string = (c8 *)userdata;
    print_string(task_string);
    free_kernel_memory(task_string);
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
        if(*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n')
            *c = '\0';

        if(prev_c == '\0' && *c)
            ++count;

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

static void
print_devicetree(void *devicetree_handle)
{
    for(DevicetreeIter iter = iterate_devicetree(devicetree_handle);
        is_devicetree_iter_valid(&iter);
        advance_devicetree_iter(&iter))
    {
        print_string(iter.dir);
        print_string(" - ");
        print_string(iter.prop);
        print_string("\r\n");
        for(um32 offset = 0; offset < iter.size; ++offset)
        {
            c8 c = (c8)iter.data[offset];
            if(32 <= c && c <= 126)
            {
                print_char(c);
            }
            else
            {
                switch(c)
                {
                    case '\0': { print_string("\\0"); } break;
                    case '\r': { print_string("\\r"); } break;
                    case '\n': { print_string("\\n"); } break;
                    case '\\': { print_string("\\\\"); } break;
                    default:
                    {
                        static c8 digits[16] =
                        {
                            '0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
                        };

                        c8 text[5] = { '\\', 'x', digits[(c >> 4) & 15], digits[c & 15], '\0' };
                        print_string(text);
                    } break;
                }
            }
        }
        print_string("\r\n");
    }
}

static void
init_kernel_state(KernelState *state, u64 devicetree_physical_addr)
{
    // memory
    void *devicetree_handle = (void *)(KERNEL_DIRECT_MAP_OFFSET + devicetree_physical_addr);
    u64 spin_table_low = 0x0000;
    u64 spin_table_high = 0x1000;
    u64 devicetree_low = devicetree_physical_addr;
    u64 devicetree_high = devicetree_physical_addr + devicetree_get_total_size(devicetree_handle);
    u64 cpio_low = 0;
    u64 cpio_high = 0;
    u64 page_table_low = 0x1000;
    u64 page_table_high = 0x4000;
    u64 kernel_image_low = (umm)&section_image_low - KERNEL_DIRECT_MAP_OFFSET;
    u64 kernel_image_high = (umm)&section_image_high - KERNEL_DIRECT_MAP_OFFSET;
    
    for(DevicetreeIter iter = iterate_devicetree(devicetree_handle);
        is_devicetree_iter_valid(&iter);
        advance_devicetree_iter(&iter))
    {
        if(string_match(iter.dir, "/chosen/"))
        {
            if(string_match(iter.prop, "linux,initrd-start"))
            {
                cpio_low = (u64)devicetree_u32(iter.data);
            }
            else if(string_match(iter.prop, "linux,initrd-end"))
            {
                cpio_high = (u64)devicetree_u32(iter.data);
            }
        }
    }
    PhysicalMemoryRegionList region_list;
    init_physical_memory_region_list(&region_list, 0x00000000, 0x3c000000);
    reserve_physical_memory_region(&region_list, spin_table_low, spin_table_high);
    reserve_physical_memory_region(&region_list, devicetree_low, devicetree_high);
    reserve_physical_memory_region(&region_list, cpio_low, cpio_high);
    reserve_physical_memory_region(&region_list, kernel_image_low, kernel_image_high);
    reserve_physical_memory_region(&region_list, page_table_low, page_table_high);
    
    init_page_pool(&state->page_pool, &region_list);
    init_memory_allocator(&state->allocator);
    
    // cpio
    state->cpio_handle = direct_mapped_virtual_address(cpio_low);
    
    // kernel page table
    u64 *stub_l0_table = alloc_kernel_memory(PAGE_TABLE_SIZE);
    for(u64 physical_addr = 0; physical_addr < 0x3f000000; physical_addr += PAGE_SIZE)
    {
        umm virtual_addr = (umm)direct_mapped_virtual_address(physical_addr);
        u64 attrib = PAGE_ATTRIB_ACCESS | PAGE_ATTRIB_MAIR_INDEX_NORMAL | PAGE_ATTRIB_L3_PAGE;
        
        u64 *entry = ensure_page_entry_exist(stub_l0_table, virtual_addr);
        *entry = physical_addr | attrib;
    }
    
    for(u64 physical_addr = 0x3f000000; physical_addr < 0x80000000; physical_addr += PAGE_SIZE)
    {
        umm virtual_addr = (umm)direct_mapped_virtual_address(physical_addr);
        u64 attrib = PAGE_ATTRIB_ACCESS | PAGE_ATTRIB_MAIR_INDEX_DEVICE | PAGE_ATTRIB_L3_PAGE;
        
        u64 *entry = ensure_page_entry_exist(stub_l0_table, virtual_addr);
        *entry = physical_addr | attrib;
    }
    
    u64 *l0_table = (u64 *)direct_mapped_virtual_address(page_table_low);
    s32 l0_count = PAGE_TABLE_SIZE >> 3;
    for(s32 l0_index = 0; l0_index < l0_count; ++l0_index)
        l0_table[l0_index] = stub_l0_table[l0_index];
    
    free_kernel_memory(stub_l0_table);
    
    // init thread
    // NOTE: the initial process must be setup before scheduler initialization
    double_link_init(&state->process_link);
    Process *process = create_empty_process();
    Thread *thread = create_empty_thread(process);
    thread->priority = ThreadPriority_normal;
    set_current_thread(thread);
    
    // timer and scheduler
    for(u32 index = 1; index < KERNEL_MAX_TIMER; ++index)
    {
        Timer *timer = state->timers + index;
        timer->next_free = index - 1;
    }
    state->first_free_timer = KERNEL_MAX_TIMER - 1;
    
    init_scheduler(&state->scheduler);
    
    // interrupt
    // TODO: implement
    for(u32 index = 1; index < KERNEL_MAX_INTERRUPT; ++index)
    {
        InterruptContext *interrupt = state->interrupts + index;
        interrupt->next_free = index - 1;
    }
    
    // bridge
    state->process_startup_bridge_offset = (umm)bridge_process_startup -
                                           (umm)&section_bridge_low;
    state->thread_cleanup_bridge_offset =  (umm)bridge_thread_cleanup -
                                           (umm)&section_bridge_low;
    state->signal_handler_cleanup_bridge_offset = (umm)bridge_signal_handler_cleanup -
                                                  (umm)&section_bridge_low;
}

static
THREAD_PROC(launch_kernel_shell)
{
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
                void *ptr = alloc_kernel_memory(size);
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
                free_kernel_memory(ptr);
            }
            else
            {
                c8 *usage = "Usage: free <ptr>\r\n";
                print_string(usage);
            }
        }
        else if(string_match(token, "reboot"))
        {
            print_string("Rebooting...\r\n");
            watchdog_reboot(1000);
        }
        else if(string_match(token, "timer"))
        {
            if(token_count == 3)
            {
                c8 *message = next_token(token);
                u64 seconds = parse_u64(next_token(message));
                u64 expiration = seconds * get_timer_frequency();

                um32 len = string_len(message);
                c8 *task_string = (c8 *)alloc_kernel_memory(len + 1);
                copy_memory(task_string, message, len + 1);

                add_timer(expiration, timer_print_string, task_string);
            }
            else
            {
                c8 *usage = "Usage: timer <message> <seconds>\r\n";
                print_string(usage);
            }
        }
        else
        {
            c8 *file_name = token;
            Process *process = create_process(file_name, 0, 0);
            if(process)
            {
                exit_thread(get_current_thread(), 0);
            }
            else
            {
                print_string("Command not found\r\n");
            }
        }
    }
}

void
kernel_main(u64 devicetree_physical_addr)
{
    // disable low virtual space
    u64 pt;
    u64 z = 0;
    __asm__ volatile("mov %0, 0x3000\n"
                     "str %1, [%0]\n"
                     "msr ttbr0_el1, %0\n"
                     : "+r"(pt) : "r"(z));
    
    enable_simd_instruction();
    irq_init();
    mini_uart_init();
    init_timer_for_core_0();
    enable_timer_access_for_el0();
    
    init_kernel_state(&g_kernel_state, devicetree_physical_addr);
    // print_devicetree(devicetree_handle);
    
    Process *process = get_current_thread()->process;
    create_thread(process, idle_thread_proc, 0, ThreadPriority_idle,
                  kilobytes(16), kilobytes(16), CreateThread_kernel);

    launch_kernel_shell(0);
}