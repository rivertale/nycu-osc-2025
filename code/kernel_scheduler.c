void switch_context(u64 *next_kernel_sp, u64 *kernel_sp);
void user_space_thread_cleanup(ExitCode exit_code);
void handle_signal(TrapFrame *trap_frame);
void kernel_space_thread_startup(void);
static ExitCode launch_kernel_shell(void *userdata);

static inline Thread *
get_current_thread(void)
{
    return (Thread *)read_tpidr_el1();
}

static inline void
set_current_thread(Thread *thread)
{
    write_tpidr_el1((umm)thread);
}

static Thread *
get_next_ready_thread(s32 min_priority)
{
    Thread *result = 0;

    KernelState *state = &g_kernel_state;
    Scheduler *scheduler = &state->scheduler;
    for(s32 priority = ThreadPriority_one_past_last - 1; priority >= min_priority; --priority)
    {
        if(!double_link_is_empty(&scheduler->ready_link[priority]))
        {
            result = (Thread *)scheduler->ready_link[priority].next;
            double_link_remove(&result->schedule_link);
            break;
        }
    }
    return result;
}

static void
schedule_thread(Thread *thread)
{
    KernelState *state = &g_kernel_state;
    Scheduler *scheduler = &state->scheduler;

    double_link_insert_at_last(&scheduler->ready_link[thread->priority], &thread->schedule_link);
}

static void
yield_physical_thread()
{
    Thread *thread = get_current_thread();

    ThreadPriority min_priority = (thread->status == ThreadStatus_exited) ?
                                  ThreadPriority_idle : thread->priority;
    Thread *next_thread = get_next_ready_thread(min_priority);
    if(next_thread)
    {
        if(thread->status != ThreadStatus_exited)
            schedule_thread(thread);
        
        set_current_user_page_table(next_thread->process->page_table);
        switch_context(&next_thread->kernel_sp, &thread->kernel_sp);
    }
}

static void
exit_thread(Thread *thread, ExitCode exit_code)
{
    assert(thread);
    thread->status = ThreadStatus_exited;
}

static Thread *
create_empty_thread(Process *process)
{
    KernelState *state = &g_kernel_state;

    Thread *thread = alloc_kernel_memory(sizeof(*thread));
    thread->id = ++state->prev_created_thread_id;
    thread->process = process;
    double_link_insert_at_last(&process->thread_link, &thread->thread_link);
    return thread;
}

static Thread *
create_thread(Process *process, ThreadProc *proc, void *param, s32 priority,
              um32 user_stack_size, um32 kernel_stack_size, u32 flags)
{
    KernelState *state = &g_kernel_state;

    Thread *thread = create_empty_thread(process);
    thread->user_stack_size = user_stack_size;
    thread->user_stack_addr =
        (umm)alloc_user_memory(process, 0, user_stack_size, AllocationType_commit,
                               MemoryPermission_read | MemoryPermission_write);
    
    thread->kernel_stack_size = kernel_stack_size;
    thread->kernel_stack_addr = (umm)alloc_kernel_memory(kernel_stack_size);
    thread->kernel_sp = thread->kernel_stack_addr + kernel_stack_size;
    thread->priority = priority;

    thread->kernel_sp -= sizeof(TrapFrame);
    TrapFrame *trap_frame = (TrapFrame *)thread->kernel_sp;
    trap_frame->lr = process->bridge_addr + state->thread_cleanup_bridge_offset;
    trap_frame->spsr_el1 = (flags & CreateThread_kernel) ?
                           KERNEL_THREAD_DEFAULT_PSTATE : USER_THREAD_DEFAULT_PSTATE;
    trap_frame->elr_el1 = (umm)proc;
    trap_frame->sp_el0 = thread->user_stack_addr + user_stack_size;
    trap_frame->x0 = (umm)param;

    thread->kernel_sp -= sizeof(SwitchFrame);
    SwitchFrame *switch_frame = (SwitchFrame *)thread->kernel_sp;
    switch_frame->lr = (umm)kernel_space_thread_startup;
    switch_frame->tpidr_el1 = (umm)thread;

    schedule_thread(thread);
    return thread;
}

static
TIMER_CALLBACK(reschedule_timer_callback)
{
    u64 freq = get_timer_frequency();
    add_timer(freq >> 5, reschedule_timer_callback, 0);
}

static
THREAD_PROC(idle_thread_proc)
{
    for(;;)
    {
        Thread *thread = get_current_thread();
        create_thread(thread->process, launch_kernel_shell, 0, ThreadPriority_normal,
                      kilobytes(16), kilobytes(16), CreateThread_kernel);
        yield_physical_thread();
    }
}

static void
init_scheduler(Scheduler *scheduler)
{
    clear_memory(scheduler, sizeof(*scheduler));
    for(s32 priority = ThreadPriority_idle; priority < ThreadPriority_one_past_last; ++priority)
    {
        double_link_init(&scheduler->ready_link[priority]);
    }

    reschedule_timer_callback(0);
}
