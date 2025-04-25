#define THREAD_PROC(name) void *name(void *userdata)
typedef THREAD_PROC(ThreadProc);

Thread *get_current_thread(void);
void set_current_thread(Thread *thread);

static void
switch_thread(Thread *next_thread, Thread *thread)
{
    
}

static void
exit_thread(void *exit_code)
{
}

static Thread *
create_empty_thread(void)
{
    KernelState *state = &g_kernel_state;
    MemoryAllocator *allocator = &state->allocator;
    
    Thread *result = alloc_memory(allocator, sizeof(*result));
    result->id = ++state->prev_created_thread_id;
    return result;
}

static ThreadId
create_thread(ThreadProc *proc, void *userdata)
{
    KernelState *state = &g_kernel_state;
    Scheduler *scheduler = &state->scheduler;
    MemoryAllocator *allocator = &state->allocator;
    
    um32 stack_size = kilobytes(64);
    u8 *stack = alloc_memory(allocator, stack_size);
    
    Thread *thread = create_empty_thread();
    thread->saved_regs[SavedReg_sp] = (umm)(stack + stack_size);
    // thread->saved_regs[SavedReg_x0] = userdata;
    thread->saved_regs[SavedReg_lr] = (umm)proc;
    thread->priority = 1;
    double_link_insert_at_last(&scheduler->ready_link[thread->priority], &thread->link);
    return thread->id;
}

static Thread *
get_next_ready_thread(void)
{
    Thread *result = 0;
    
    KernelState *state = &g_kernel_state;
    Scheduler *scheduler = &state->scheduler;
    for(s32 priority = MAX_SCHEDULE_PRIORITY; priority >= 0; --priority)
    {
        if(!double_link_is_empty(&scheduler->ready_link[priority]))
        {
            result = (Thread *)scheduler->ready_link[priority].next;
            double_link_remove(&result->link);
            break;
        }
    }
    return result;
}

static void
schedule(void)
{
    Thread *next_thread = get_next_ready_thread();
    if(next_thread)
    {
        Thread *thread = get_current_thread();
        switch_thread(next_thread, thread);
    }
}

static THREAD_PROC(idle_thread_proc)
{
    for(;;)
    {
        schedule();
    }
}
