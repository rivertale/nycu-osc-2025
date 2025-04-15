#include "kernel.h"
#include "kernel_cpio.c"
#include "kernel_devicetree.c"
#include "kernel_mailbox.c"
#include "kernel_watchdog.c"
#include "uart.c"

static void print_buffer(c8 *buffer, umm size);
static void print_string(c8 *message);
static void print_hex32(u32 value);
static void print_hex64(u64 value);
static void print_u64(u64 value);
#include "kernel_timer.c"
#include "kernel_interrupt.c"
#include "kernel_memory.c"

static void
read_console(void *buffer, u64 size)
{
    KernelState *state = &g_kernel_state;
    mini_uart_enable_read_interrupt();

    u8 *byte = (u8 *)buffer;
    while(size > 0)
    {
        if(state->read_cur0 != state->read_cur1)
        {
            *byte++ = state->read_buffer[state->read_cur0];
            state->read_cur0 = (state->read_cur0 + 1) & KERNEL_IO_BUFFER_MASK;
            --size;
        }
    }

    mini_uart_disable_read_interrupt();
}

static void
write_console(void *buffer, u64 size)
{
    KernelState *state = &g_kernel_state;

    u8 *byte = (u8 *)buffer;
    while(size > 0)
    {
        um32 next_write_cur1 = (state->write_cur1 + 1) & KERNEL_IO_BUFFER_MASK;
        while(size > 0 && next_write_cur1 != state->write_cur0)
        {
            state->write_buffer[state->write_cur1] = *byte++;
            state->write_cur1 = next_write_cur1;
            next_write_cur1 = (state->write_cur1 + 1) & KERNEL_IO_BUFFER_MASK;
            --size;
        }

        mini_uart_enable_write_interrupt();
    }
}

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
DEVICETREE_CALLBACK(init_kernel_addr_range)
{
    b32 result = 0;

    KernelState *state = (void *)userdata;

    if(string_match(path, "/chosen/"))
    {
        if(string_match(prop_name, "linux,initrd-start"))
        {
            result = 1;
            state->cpio_begin = (void *)(umm)devicetree_u32(prop);
        }
        else if(string_match(prop_name, "linux,initrd-end"))
        {
            result = 1;
            state->cpio_end = (void *)(umm)devicetree_u32(prop);
        }
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

    for(um32 index = 0; index < prop_size; ++index)
    {
        c8 c = ((c8 *)prop)[index];
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
                    static c8 hex_digits[16] =
                    {
                        '0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
                    };

                    c8 byte[5] =
                    {
                        '\\', 'x',
                        hex_digits[(c >> 4) & 15],
                        hex_digits[(c >> 0) & 15],
                        '\0'
                    };
                    print_string(byte);
                } break;
            }
        }
    }
    print_string("\r\n");
    return 0;
}

static
KERNEL_TIMER_CALLBACK(timer_print_string)
{
    PrintStringTask *task = (PrintStringTask *)userdata;
    print_string(task->string);
    free_memory(task->allocator, task->string);
    free_memory(task->allocator, task);
}

static
KERNEL_TIMER_CALLBACK(timer_tell_time)
{
    u64 freq = get_timer_frequency();
    u64 expiration = 2 * freq;
    add_timer(expiration, timer_tell_time, 0);
    print_string("Seconds after boot: ");
    print_u64(get_timer_current_tick() / freq);
    print_string("\r\n");
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
irq_init(void)
{
    *(vu32 *)IRQ_ENABLED_1 |= IRQ_INTERRUPT_AUX;
}

static void
init_kernel_state(KernelState *state, void *devicetree_addr)
{
    g_kernel_state.devicetree_begin = devicetree_addr;
    g_kernel_state.devicetree_end = devicetree_addr + devicetree_get_total_size(devicetree_addr);

    u32 total_device_addr_count = (&state->last_device_addr - &state->first_device_addr + 1);
    if(devicetree_traverse(devicetree_addr, init_kernel_addr_range, &g_kernel_state) !=
       total_device_addr_count)
    {
        invalid_code_path;
    }

    for(u32 index = 1; index < KERNEL_MAX_TIMER; ++index)
    {
        Timer *timer = state->timers + index;
        timer->next_free = index - 1;
    }
    state->first_free_timer = KERNEL_MAX_TIMER - 1;

    for(u32 index = 1; index < KERNEL_MAX_INTERRUPT; ++index)
    {
        InterruptContext *interrupt = state->interrupts + index;
        interrupt->next_free = index - 1;
    }
}

static void
enable_simd_instruction(void)
{
    u64 cpacr_el1 = 0;
    __asm__ volatile ("mrs %0, cpacr_el1" : "=r"(cpacr_el1));
    cpacr_el1 |= (3 << 20);
    __asm__ volatile ("msr cpacr_el1, %0" :: "r"(cpacr_el1));
}

void
kernel_main(void *devicetree_addr)
{
    enable_simd_instruction();
    irq_init();
    mini_uart_init();
    init_timer_for_core_0();

    init_kernel_state(&g_kernel_state, devicetree_addr);
    // devicetree_traverse(devicetree_addr, print_devicetree, 0);

    umm boot_arena_size = PAGE_MAX_ALLOC_SIZE;
    void *boot_arena_begin = (void *)0x10000000;
    void *boot_arena_end = (void *)((umm)boot_arena_begin + boot_arena_size);
    BootArena *boot_arena = bootstrap_boot_arena(boot_arena_begin, boot_arena_end);

    void *spin_table_begin = (void *)0x0000;
    void *spin_table_end = (void *)0x1000;

    MemoryRegionList *region_list = push_size(boot_arena, sizeof(*region_list));
    init_memory_region_list(region_list, (void *)0x00000000, (void *)0x3c000000);
    reserve_memory_region(region_list, spin_table_begin, spin_table_end);
    reserve_memory_region(region_list, &kernel_image_begin, &kernel_image_end);
    reserve_memory_region(region_list,
                          g_kernel_state.devicetree_begin, g_kernel_state.devicetree_end);
    reserve_memory_region(region_list, g_kernel_state.cpio_begin, g_kernel_state.cpio_end);
    reserve_memory_region(region_list, boot_arena_begin, boot_arena_end);

    PagePool page_pool;
    init_page_pool(&page_pool, region_list);

    MemoryAllocator allocator;
    init_memory_allocator(&allocator, &page_pool);


    // add_timer(0, timer_tell_time, 0);
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
                void *ptr = alloc_memory(&allocator, size);
                // print_hex64((umm)ptr);
                // print_string("\r\n");
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
                free_memory(&allocator, ptr);
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
                PrintStringTask *task = alloc_memory(&allocator, sizeof(*task));
                task->allocator = &allocator;
                task->string = (c8 *)alloc_memory(&allocator, len + 1);
                copy_memory(task->string, message, len + 1);

                add_timer(expiration, timer_print_string, task);
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
            void *handle = cpio_find_file(file_name);
            if(handle)
            {
                um32 len;
                c8 *content = (c8 *)cpio_get_file_content(handle, &len);
                void *load_addr = (void *)USER_SPACE_BEGIN;
                void *stack_end = (void *)USER_SPACE_END;
                copy_memory(load_addr, content, len);
                __asm__ volatile("mov x0, 0x340\n"
                                 "msr spsr_el1, x0\n"
                                 "msr elr_el1, %0\n"
                                 "msr sp_el0, %1\n"
                                 "eret\n"
                                 :: "r"(load_addr), "r"(stack_end));
            }
            else
            {
                print_string("Unrecognized command: ");
                print_string(token);
                print_string("\r\n");
            }
        }
    }
}