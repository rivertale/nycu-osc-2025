static inline void
init_timer_for_core_0(void)
{
    __asm__ volatile("mov x0, 2\n"
                     "ldr x1, =%0\n"
                     "str w0, [x1]\n"
                     :: "i"(CORE0_TIMER_IRQ_CTRL));
}

static inline void
enable_timer(void)
{
    __asm__ volatile("mov x0, 1\n"
                     "msr cntp_ctl_el0, x0\n");
}

static inline void
disable_timer(void)
{
    __asm__ volatile("mov x0, 0\n"
                     "msr cntp_ctl_el0, x0\n");
}

static inline void
set_timer_expiration(u64 tick)
{
    __asm__ volatile("msr cntp_cval_el0, %0" :: "r"(tick));
}

static inline u64
get_timer_frequency(void)
{
    u64 result = 0;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(result));
    return result;
}

static inline u64
get_timer_current_tick(void)
{
    u64 result = 0;
    __asm__ volatile ("mrs %0, cntpct_el0" : "=r"(result));
    return result;
}

static b32
add_timer(u64 expiration, KernelTimerCallback *callback, void *userdata)
{
    b32 result = 0;
    
    KernelState *state = &g_kernel_state;
    if(state->first_free_timer)
    {
        result = 1;
        
        u32 index = state->first_free_timer;
        Timer *timer = state->timers + index;
        state->first_free_timer = timer->next_free;
        timer->tick = get_timer_current_tick() + expiration;
        timer->callback = callback;
        timer->userdata = userdata;
        
        u32 *index_ptr = &state->first_expired_timer;
        while(*index_ptr)
        {
            Timer *link = state->timers + *index_ptr;
            if(link->tick > timer->tick)
                break;
            index_ptr = &link->next_expired;
        }
        timer->next_expired = *index_ptr;
        *index_ptr = index;
        
        if(state->first_expired_timer == index)
        {
            set_timer_expiration(timer->tick);
            enable_timer();
        }
        
    }
    
    return result;
}
