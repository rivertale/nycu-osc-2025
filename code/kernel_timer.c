static inline void
init_timer_for_core_0(void)
{
    *(vu32 *)IRQ_CORE0_TIMER_CTRL = 2;
}

static inline void
enable_timer_access_for_el0(void)
{
    u64 reg = read_cntkctl_el1();
    write_cntkctl_el1(reg | 0x1);
}

static inline void
enable_timer(void)
{
    u64 reg = read_cntp_ctl_el0();
    write_cntp_ctl_el0(reg | 0x1);
}

static inline void
disable_timer(void)
{
    u64 reg = read_cntp_ctl_el0();
    write_cntp_ctl_el0(reg & ~0x1);
}

static inline void
set_timer_expiration(u64 tick)
{
    write_cntp_cval_el0(tick);
}

static inline u64
get_timer_frequency(void)
{
    u64 result = read_cntfrq_el0();
    return result;
}

static inline u64
get_timer_current_tick(void)
{
    u64 result = read_cntpct_el0();
    return result;
}

static b32
add_timer(u64 expiration, TimerCallback *callback, void *userdata)
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
