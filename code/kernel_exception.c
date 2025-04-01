static b32
add_interrupt(InterruptKind kind)
{
    b32 result = 0;
    
    KernelState *state = &g_kernel_state;
    if(state->first_free_interrupt)
    {
        result = 1;
        
        u32 index = state->first_free_interrupt;
        InterruptContext *interrupt = state->interrupts + index;
        state->first_free_interrupt = interrupt->next_free;
        interrupt->kind = kind;
        
        u32 *index_ptr = &state->first_prioritized_interrupt;
        while(*index_ptr)
        {
            InterruptContext *link = state->interrupts + *index_ptr;
            if(link->priority < interrupt->priority)
                break;
            index_ptr = &link->next_prioritized;
        }
        *index_ptr = index;
    }
    return result;
}

void
exception_handler_el1_cur_sync(void)
{
    u64 spsr_el1 = 0;
    u64 elr_el1 = 0;
    u64 esr_el1 = 0;
    
    __asm__ volatile("mrs %0, spsr_el1\n"
                     "mrs %1, elr_el1\n"
                     "mrs %2, esr_el1\n"
                     : "=r"(spsr_el1), "=r"(elr_el1), "=r"(esr_el1));
    
    print_string("spsr_el1=");
    print_hex64(spsr_el1);
    print_string("\r\n");
    print_string("elr_el1=");
    print_hex64(elr_el1);
    print_string("\r\n");
    print_string("esr_el1=");
    print_hex64(esr_el1);
    print_string("\r\n");
}

void
exception_handler_el1_cur_irq(void)
{
    KernelState *state = &g_kernel_state;
    
    u32 pending = *(vu32 *)IRQ_PENDING_1;
    if(pending & IRQ_AUX_INT)
    {
        *(vu32 *)IRQ_ENABLED_1 &= ~IRQ_AUX_INT;
        
        // UART
        if(*(vu32 *)AUX_MU_IIR_REG & 0x2)
        {
            if(state->write_cur0 != state->write_cur1)
            {
                *(vu32 *)AUX_MU_IO_REG = state->write_buffer[state->write_cur0];
                state->write_cur0 = (state->write_cur0 + 1) & KERNEL_IO_BUFFER_MASK;
            }
            else
            {
                mini_uart_disable_write_exception();
            }
        }
        else if(*(vu32 *)AUX_MU_IIR_REG & 0x4)
        {
            g_kernel_state.read_buffer[state->read_cur1] = *(vu32 *)AUX_MU_IO_REG;
            state->read_cur1 = (state->read_cur1 + 1) & KERNEL_IO_BUFFER_MASK;
        }
        
        *(vu32 *)IRQ_ENABLED_1 |= IRQ_AUX_INT;
    }
    else
    {
        // timer
        assert(state->first_expired_timer);
        Timer *expired = state->timers + state->first_expired_timer;
        expired->callback(expired->userdata);
        
        expired->next_free = state->first_free_timer;
        state->first_free_timer = state->first_expired_timer;
        state->first_expired_timer = expired->next_expired;
        
        if(state->first_expired_timer)
        {
            Timer *first = state->timers + state->first_expired_timer;
            set_timer_expiration(first->tick);
        }
        else
        {
            disable_timer();
        }
    }
}

void
exception_handler_el1_low_sync(void)
{
    u64 spsr_el1 = 0;
    u64 elr_el1 = 0;
    u64 esr_el1 = 0;
    
    __asm__ volatile("mrs %0, spsr_el1\n"
                     "mrs %1, elr_el1\n"
                     "mrs %2, esr_el1\n"
                     : "=r"(spsr_el1), "=r"(elr_el1), "=r"(esr_el1));
    
    print_string("spsr_el1=");
    print_hex64(spsr_el1);
    print_string("\r\n");
    print_string("elr_el1=");
    print_hex64(elr_el1);
    print_string("\r\n");
    print_string("esr_el1=");
    print_hex64(esr_el1);
    print_string("\r\n");
}

void
exception_handler_el1_low_irq(void)
{
    u64 spsr_el1 = 0;
    u64 elr_el1 = 0;
    u64 esr_el1 = 0;
    
    __asm__ volatile("mrs %0, spsr_el1\n"
                     "mrs %1, elr_el1\n"
                     "mrs %2, esr_el1\n"
                     : "=r"(spsr_el1), "=r"(elr_el1), "=r"(esr_el1));
    
    print_string("spsr_el1=");
    print_hex64(spsr_el1);
    print_string("\r\n");
    print_string("elr_el1=");
    print_hex64(elr_el1);
    print_string("\r\n");
    print_string("esr_el1=");
    print_hex64(esr_el1);
    print_string("\r\n");
}

void
exception_handler_default(void)
{
    print_string("unknown exception\r\n");
}
