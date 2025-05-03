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
    MemoryAllocator *allocator = &state->allocator;

    Process *process = alloc_memory(allocator, sizeof(*process));
    process->id = ++state->prev_created_process_id;
    double_link_init(&process->thread_link);
    double_link_insert_at_last(&state->process_link, &process->process_link);

    return process;
}

static void
create_process_startup(ProcessStartup *startup, ProcessProc *proc, s32 arg_count, c8 **args)
{
    KernelState *state = &g_kernel_state;
    MemoryAllocator *allocator = &state->allocator;

    startup->proc = proc;
    startup->arg_count = arg_count;
    startup->args = alloc_memory(allocator, arg_count * sizeof(c8 *));
    for(s32 index = 0; index < arg_count; ++index)
    {
        umm len = string_len(args[index]);
        startup->args[index] = alloc_memory(allocator, len + 1);
        copy_memory(startup->args[index], args[index], len + 1);
    }
}

static Process *
create_process(c8 *image_path, s32 arg_count, c8 **args)
{
    KernelState *state = &g_kernel_state;
    MemoryAllocator *allocator = &state->allocator;

    Process *process = 0;
    void *handle = cpio_find_file(image_path);
    if(handle)
    {
        um32 image_size;
        u8 *image = cpio_get_file_content(handle, &image_size);

        process = create_empty_process();
        process->image_size = image_size;
        process->image_addr = (umm)alloc_memory(allocator, image_size);
        copy_memory((void *)process->image_addr, image, image_size);
        create_process_startup(&process->startup, (ProcessProc *)process->image_addr,
                               arg_count, args);
        create_thread(process, user_space_process_startup, &process->startup,
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
