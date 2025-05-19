static umm
read_console(void *buffer, umm size)
{
    KernelState *state = &g_kernel_state;

    mini_uart_enable_read_interrupt();

    u8 *byte = (u8 *)buffer;
    umm byte_left = size;
    while(byte_left > 0)
    {
        if(state->read_cur0 != state->read_cur1)
        {
            *byte++ = state->read_buffer[state->read_cur0];
            state->read_cur0 = (state->read_cur0 + 1) & KERNEL_IO_BUFFER_MASK;
            --byte_left;
        }
    }
    mini_uart_disable_read_interrupt();
    return size;
}

static umm
write_console(void *buffer, umm size)
{
    KernelState *state = &g_kernel_state;

    u8 *byte = (u8 *)buffer;
    umm byte_left = size;
    while(byte_left > 0)
    {
        um32 next_write_cur1 = (state->write_cur1 + 1) & KERNEL_IO_BUFFER_MASK;
        while(byte_left > 0 && next_write_cur1 != state->write_cur0)
        {
            state->write_buffer[state->write_cur1] = *byte++;
            state->write_cur1 = next_write_cur1;
            next_write_cur1 = (state->write_cur1 + 1) & KERNEL_IO_BUFFER_MASK;
            --byte_left;
        }

        mini_uart_enable_write_interrupt();
    }
    return size;
}

static void
syscall_get_process_id(TrapFrame *trap_frame)
{
    Thread *thread = get_current_thread();
    trap_frame->x0 = thread->process->id;
}

static void
syscall_read_console(TrapFrame *trap_frame)
{
    void *buffer = (void *)trap_frame->x0;
    u64 size = (u64)trap_frame->x1;
    trap_frame->x0 = read_console(buffer, size);
}

static void
syscall_write_console(TrapFrame *trap_frame)
{
    void *buffer = (void *)trap_frame->x0;
    u64 size = (u64)trap_frame->x1;
    trap_frame->x0 = write_console(buffer, size);
}

static void
syscall_exec(TrapFrame *trap_frame)
{
    c8 *image_path = (void *)trap_frame->x0;
    c8 **args = (c8 **)trap_frame->x1;
    s32 arg_count = 0;
    if(args)
    {
        for(c8 **arg = args; *arg; ++arg)
            ++arg_count;
    }

    KernelState *state = &g_kernel_state;
    MemoryAllocator *allocator = &state->allocator;
    void *handle = cpio_find_file(image_path);
    if(handle)
    {
        umm args_size = 0;
        for(s32 index = 0; index < arg_count; ++index)
            args_size += string_len(args[index]) + 1;
        
        c8 **kernel_args = (c8 **)alloc_kernel_memory(arg_count * sizeof(c8 *));
        c8 *kernel_arg_memory = (c8 *)alloc_kernel_memory(args_size);
        
        c8 *kernel_arg_cur = kernel_arg_memory;
        for(s32 index = 0; index < arg_count; ++index)
        {
            umm len = string_len(args[index]);
            copy_memory(kernel_arg_cur, args[index], len + 1);
            
            kernel_args[index] = kernel_arg_cur;
            kernel_arg_cur += len + 1;
        }
        
        Thread *thread = get_current_thread();
        Process *process = thread->process;
        
        while(process->memory_tree.root)
            free_user_memory(process, (void *)process->memory_tree.root->low);
        invalidate_entire_tlb();
        
        um32 image_size;
        u8 *image = cpio_get_file_content(handle, &image_size);
        load_initial_process_image(process, image, image_size, arg_count, kernel_args);
        
        
        free_kernel_memory(kernel_arg_memory);
        free_kernel_memory(kernel_args);
        
        // we freed all user stacks, reallocate a stack for the calling thread
        thread->user_stack_addr =
            (umm)alloc_user_memory(process, 0, thread->user_stack_size,
                                   AllocationType_demand,
                                   MemoryPermission_read | MemoryPermission_write, 0, 0);

        process->pending_signal_cur0 = 0;
        process->pending_signal_cur1 = 0;
        for(Signal signal = Signal_none; signal < Signal_one_past_last; ++signal)
            process->signal_handlers[signal] = 0;

        for(Link *link = process->thread_link.next;
            link != &process->thread_link;
            link = link->next)
        {
            Thread *it = ptr_from_field(link, Thread, thread_link);
            if(it != thread)
                exit_thread(it, 0);
        }

        trap_frame->lr = process->bridge_addr + state->thread_cleanup_bridge_offset;
        trap_frame->spsr_el1 = USER_THREAD_DEFAULT_PSTATE;
        trap_frame->elr_el1 = process->bridge_addr + state->process_startup_bridge_offset;
        trap_frame->sp_el0 = thread->user_stack_addr + thread->user_stack_size;
        trap_frame->x0 = (umm)process->startup;
    }
    else
    {
        trap_frame->x0 = -1;
    }
}

static void
syscall_fork(TrapFrame *trap_frame)
{
    // NOTE: it's impossible to implement this function without a trap frame, we can't rely on
    // trap_frame in old_thread, since that trap_frame is the TrapFrame for the latest swapped out,
    // we might get swapped out during syscall, and the trap_frame in old_thread will not be the
    // same trap_frame as when we call this syscall.
    KernelState *state = &g_kernel_state;
    MemoryAllocator *allocator = &state->allocator;

    Thread *old_thread = get_current_thread();
    Process *old_process = (Process *)old_thread->process;
    TrapFrame *old_trap_frame = trap_frame;

    Process *new_process = create_empty_process();
    new_process->image_size = old_process->image_size;
    new_process->image_addr = old_process->image_addr;
    
    for(Signal signal = Signal_none; signal < Signal_one_past_last; ++signal)
        new_process->signal_handlers[signal] = old_process->signal_handlers[signal];
    
    
    for(L3PageEntryIter iter = iterate_l3_page_entry(old_process->page_table);
        is_l3_page_entry_iter_valid(&iter);
        advance_l3_page_entry_iter(&iter))
    {
        *iter.entry = (*iter.entry & ~PAGE_ATTRIB_RW_MASK) | PAGE_ATTRIB_RO_EL0;
    }
    duplicate_user_space(new_process, old_process);
    
    
    Thread *new_thread = create_empty_thread(new_process);
    new_thread->user_stack_size = old_thread->user_stack_size;
    new_thread->user_stack_addr = old_thread->user_stack_addr;
    new_thread->kernel_stack_size = old_thread->kernel_stack_size;
    new_thread->kernel_stack_addr = (umm)alloc_kernel_memory(old_thread->kernel_stack_size);
    new_thread->priority = old_thread->priority;
    new_thread->kernel_sp = new_thread->kernel_stack_addr + new_thread->kernel_stack_size;

    new_thread->kernel_sp -= sizeof(TrapFrame);
    TrapFrame *new_trap_frame = (TrapFrame *)new_thread->kernel_sp;
    copy_memory(new_trap_frame, old_trap_frame, sizeof(*old_trap_frame));
    new_trap_frame->spsr_el1 = USER_THREAD_DEFAULT_PSTATE;
    new_trap_frame->x0 = 0;

    new_thread->kernel_sp -= sizeof(SwitchFrame);
    SwitchFrame *new_switch_frame = (SwitchFrame *)new_thread->kernel_sp;
    new_switch_frame->lr = (umm)kernel_space_thread_startup;
    new_switch_frame->tpidr_el1 = (umm)new_thread;

    old_trap_frame->x0 = new_process->id;
    schedule_thread(new_thread);
}

static void
syscall_exit_current_process(TrapFrame *trap_frame)
{
    Thread *thread = get_current_thread();
    ExitCode exit_code = trap_frame->x0;
    exit_process(thread->process, exit_code);
    yield_physical_thread();
}

static void
syscall_mailbox_query(TrapFrame *trap_frame)
{
    u8 channel = (u8)trap_frame->x0;
    u32 *message = (u32 *)trap_frame->x1;
    trap_frame->x0 = mailbox_query(channel, message);
}

static void
syscall_send_kill_signal(TrapFrame *trap_frame)
{
    ProcessId process_id = (ProcessId)trap_frame->x0;
    Process *process = find_process(process_id);
    if(process)
    {
        trap_frame->x0 = send_signal(process, Signal_kill) ? 0 : -1;
    }
    else
    {
        trap_frame->x0 = -1;
    }
}

static void
syscall_set_signal_handler(TrapFrame *trap_frame)
{
    Thread *thread = get_current_thread();
    Signal signal = (Signal)trap_frame->x0;
    SignalHandler *handler = (SignalHandler *)trap_frame->x1;
    trap_frame->x0 = (umm)set_signal_handler(thread->process, signal, handler);
}

static void
syscall_send_signal(TrapFrame *trap_frame)
{
    ProcessId process_id = (ProcessId)trap_frame->x0;
    Signal signal = (Signal)trap_frame->x1;
    Process *process = find_process(process_id);

    if(process)
    {
        trap_frame->x0 = send_signal(process, signal) ? 0 : -1;
    }
    else
    {
        trap_frame->x0 = -1;
    }
}

static void
syscall_alloc_memory(TrapFrame *trap_frame)
{
    Thread *thread = get_current_thread();
    Process *process = thread->process;
    
    void *addr = (void *)trap_frame->x0;
    umm size = trap_frame->x1;
    s32 prot = trap_frame->x2;
    s32 flags = trap_frame->x3;
    void *file = (void *)trap_frame->x4;
    s32 file_offset = trap_frame->x5;
    
    if(flags & 0x20) // MAP_ANONYMOUS
    {
        AllocationType type = AllocationType_demand;
        if(flags & 0x8000) //MAP_POPULATE
            type = AllocationType_commit;
        
        MemoryPermission permission = MemoryPermission_none;
        if(prot & 0x1) // PROT_READ
            permission |= MemoryPermission_read;
        if(prot & 0x2) // PROT_WRITE
            permission |= MemoryPermission_write;
        if(prot & 0x4) // PROT_EXEC
            permission |= MemoryPermission_execute;
        
        trap_frame->x0 = (umm)alloc_user_memory(process, addr, size, type, permission,
                                                file, file_offset);
    }
    else
    {
        trap_frame->x0 = -1;
    }
        
}

static void
syscall_exit_signal_handler(TrapFrame *trap_frame)
{
    Thread *thread = get_current_thread();

    SignalFrame *signal_frame = (SignalFrame *)trap_frame->sp_el0;
    copy_from_signal_frame(trap_frame, signal_frame);

    // HACK: if the trap frame is at the start of the kernel stack, then we must be returning to
    // user mode; otherwise, we must be returning to kernel mode. note that a kernel thread won't
    // call sigreturn(), so the calling thread must be a user thread.
    //
    // the user thread might be inside a syscall (in kernel mode) when invoking signal handler,
    // we set it to user mode before entering the signal handler. as a result, we can't tell if
    // the thread was in user mode or kernel mode before the signal handler, the hack is used to
    // infer the lost information

    if((u64)trap_frame + sizeof(*trap_frame) ==
       thread->kernel_stack_addr + thread->kernel_stack_size)
    {
        trap_frame->spsr_el1 = USER_THREAD_DEFAULT_PSTATE;
    }
    else
    {
        trap_frame->spsr_el1 = KERNEL_THREAD_DEFAULT_PSTATE;
    }
}

static void
syscall_exit_current_thread(TrapFrame *trap_frame)
{
    Thread *thread = get_current_thread();
    ExitCode exit_code = trap_frame->x0;
    exit_thread(thread, exit_code);
    yield_physical_thread();
}

static void
handle_syscall(TrapFrame *trap_frame)
{
    u32 syscall_num = trap_frame->x8;
    switch(syscall_num)
    {
        case Syscall_get_process_id:
        {
            syscall_get_process_id(trap_frame);
        } break;
        case Syscall_read_console:
        {
            syscall_read_console(trap_frame);
        } break;
        case Syscall_write_console:
        {
            syscall_write_console(trap_frame);
        } break;
        case Syscall_exec:
        {
            syscall_exec(trap_frame);
        } break;
        case Syscall_fork:
        {
            syscall_fork(trap_frame);
        } break;
        case Syscall_exit_current_process:
        {
            syscall_exit_current_process(trap_frame);
        } break;
        case Syscall_mailbox_query:
        {
            syscall_mailbox_query(trap_frame);
        } break;
        case Syscall_send_kill_signal:
        {
            syscall_send_kill_signal(trap_frame);
        } break;
        case Syscall_set_signal_handler:
        {
            syscall_set_signal_handler(trap_frame);
        } break;
        case Syscall_send_signal:
        {
            syscall_send_signal(trap_frame);
        } break;
        case Syscall_alloc_memory:
        {
            syscall_alloc_memory(trap_frame);
        } break;
        case Syscall_exit_signal_handler:
        {
            syscall_exit_signal_handler(trap_frame);
        } break;
        case Syscall_exit_current_thread:
        {
            syscall_exit_current_thread(trap_frame);
        } break;
    }
}
