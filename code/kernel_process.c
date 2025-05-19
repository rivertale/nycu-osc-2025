static void
exit_process(Process *process, ExitCode exit_code)
{
    for(Link *link = process->thread_link.next;
        link != &process->thread_link;
        link = link->next)
    {
        Thread *thread = ptr_from_field(link, Thread, thread_link);
        exit_thread(thread, exit_code);
    }
}

static Process *
find_process(ProcessId id)
{
    KernelState *state = &g_kernel_state;

    Process *result = 0;
    for(Link *link = state->process_link.next;
        link != &state->process_link;
        link = link->next)
    {
        Process *it = ptr_from_field(link, Process, process_link);
        if(it->id == id)
        {
            result = it;
            break;
        }
    }
    return result;
}

static Process *
create_empty_process(void)
{
    KernelState *state = &g_kernel_state;

    Process *process = alloc_kernel_memory(sizeof(*process));
    process->id = ++state->prev_created_process_id;
    process->page_table = alloc_pages(PAGE_TABLE_SIZE);
    init_virtual_memory_tree(&process->memory_tree);
    double_link_init(&process->thread_link);
    double_link_insert_at_last(&state->process_link, &process->process_link);
    
    return process;
}

static void
create_process_startup(Process *process, ProcessProc *proc, s32 arg_count, c8 **kernel_args)
{
    KernelState *state = &g_kernel_state;
    
    umm user_size = sizeof(ProcessStartup) + arg_count * sizeof(c8 *);
    for(s32 index = 0; index < arg_count; ++index)
        user_size += string_len(kernel_args[index]) + 1;
    
    ProcessStartup kernel_startup = {0};
    ProcessStartup *user_startup =
        alloc_user_memory(process, 0, user_size, AllocationType_commit,
                          MemoryPermission_read | MemoryPermission_write, 0, 0);
    c8 **user_args = (c8 **)(user_startup + 1);
    c8 *user_arg_cur = (c8 *)(user_args + arg_count);
    
    kernel_startup.proc = proc;
    kernel_startup.arg_count = arg_count;
    kernel_startup.args = user_args;
    
    for(s32 index = 0; index < arg_count; ++index)
    {
        umm len = string_len(kernel_args[index]);
        copy_to_user(process, user_arg_cur, kernel_args[index], len + 1);
        kernel_startup.args[index] = user_arg_cur;
        user_arg_cur += len + 1;
    }
    
    copy_to_user(process, user_startup, &kernel_startup, sizeof(*user_startup));
    process->startup = user_startup;
}

static void
load_initial_process_image(Process *process, u8 *image, um32 image_size,
                           s32 arg_count, c8 **kernel_args)
{
    // TODO: it should be read + execute, but we need to find a way to write to it in kernel
    u32 execute_permission = MemoryPermission_read | MemoryPermission_write |
                             MemoryPermission_execute;
    
    // NOTE: since mailbox return physical address directly, we must map peripheral's virtual
    // address exactly the same as physical address
    umm peripheral_size = PERIPHERAL_HIGH - PERIPHERAL_LOW;
    u64 peripheral_physical_addr = direct_mapped_physical_address(PERIPHERAL_LOW);
    void *peripheral = map_device_memory(process,
                                         peripheral_physical_addr, peripheral_physical_addr,
                                         peripheral_size);
    assert(peripheral);
    
    process->image_size = image_size;
    process->image_addr = (umm)alloc_user_memory(process, 0, image_size, AllocationType_demand,
                                                 execute_permission, image, 0);
    
    umm bridge_physical_addr = direct_mapped_physical_address((umm)&section_bridge_low);
    umm bridge_size = (umm)&section_bridge_high - (umm)&section_bridge_low;
    process->bridge_addr = (umm)map_user_memory(process,
                                                bridge_physical_addr, bridge_size,
                                                AllocationType_commit, execute_permission);
    
    create_process_startup(process, (ProcessProc *)process->image_addr, arg_count, kernel_args);
}

// NOTE: kernel_args must be in kernel space
static Process *
create_process(c8 *image_path, s32 arg_count, c8 **kernel_args)
{
    KernelState *state = &g_kernel_state;

    Process *process = 0;
    void *handle = cpio_find_file(image_path);
    if(handle)
    {
        um32 image_size;
        u8 *image = cpio_get_file_content(handle, &image_size);
        
        process = create_empty_process();
        load_initial_process_image(process, image, image_size, arg_count, kernel_args);
        
        create_thread(process,
                      (ThreadProc *)(process->bridge_addr + state->process_startup_bridge_offset),
                      process->startup,
                      ThreadPriority_normal, kilobytes(16), kilobytes(16), 0);
    }
    return process;
}

static SignalHandler *
set_signal_handler(Process *process, Signal signal, SignalHandler *handler)
{
    SignalHandler *prev_handler = process->signal_handlers[signal];
    process->signal_handlers[signal] = handler;
    return prev_handler;
}

static b32
send_signal(Process *process, Signal signal)
{
    b32 result = 0;
    u32 next_pending_signal_cur1 = (process->pending_signal_cur1 + 1) & PENDING_SIGNAL_BUFFER_MASK;
    if(next_pending_signal_cur1 != process->pending_signal_cur0)
    {
        result = 1;
        process->pending_signals[process->pending_signal_cur1] = signal;
        process->pending_signal_cur1 = next_pending_signal_cur1;
    }
    return result;
}
