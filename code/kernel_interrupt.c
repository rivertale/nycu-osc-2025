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

static void
handle_irq_interrupt(void)
{
    // TODO: accept new interrupt in handler
    KernelState *state = &g_kernel_state;

    u32 pending_0 = *(vu32 *)IRQ_PENDING_0;
    if(pending_0 & IRQ_PENDING_0_SET_1)
    {
        // irq pending 1 has bit set
        u32 pending_1 = *(vu32 *)IRQ_PENDING_1;
        if(pending_1 & IRQ_INTERRUPT_AUX)
        {
            // aux interrupt is pending
            if(*(vu32 *)AUX_IRQ & AUX_IRQ_MINI_UART)
            {
                // mini uart interrupt is pending
                u32 iir = *(vu32 *)AUX_MU_IIR_REG;
                assert((iir & AUX_MU_IIR_PENDING) == 0);

                // AUX_MU_IIR_TRANSMIT and AUX_MU_IIR_RECEIVE won't be set at the same time
                if(iir & AUX_MU_IIR_TRANSMIT)
                {
                    if(state->write_cur0 != state->write_cur1)
                    {
                        *(vu32 *)AUX_MU_IO_REG = state->write_buffer[state->write_cur0];
                        state->write_cur0 = (state->write_cur0 + 1) & KERNEL_IO_BUFFER_MASK;
                    }
                    else
                    {
                        mini_uart_disable_write_interrupt();
                    }
                }
                else if(iir & AUX_MU_IIR_RECEIVE)
                {
                    state->read_buffer[state->read_cur1] = *(vu32 *)AUX_MU_IO_REG;
                    state->read_cur1 = (state->read_cur1 + 1) & KERNEL_IO_BUFFER_MASK;
                }

                *(vu32 *)IRQ_ENABLED_1 |= IRQ_INTERRUPT_AUX;
            }
        }
    }

    if(pending_0 & IRQ_PENDING_0_SET_2)
    {
        // irq pending 2 has bit set
        do_nothing;
    }

    u32 irq_core0_source = *(vu32 *)IRQ_CORE0_INTERRUPT_SOURCE;
    if(irq_core0_source & IRQ_CORE_INTERRUPT_SOURCE_CNTPNSIRQ)
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
interrupt_handler_el1_cur_sync(void)
{
    u64 spsr_el1 = 0;
    u64 elr_el1 = 0;
    u64 esr_el1 = 0;

    __asm__ volatile("mrs %0, spsr_el1\n"
                     "mrs %1, elr_el1\n"
                     "mrs %2, esr_el1\n"
                     : "=r"(spsr_el1), "=r"(elr_el1), "=r"(esr_el1));

    u32 iss = (esr_el1 >> 0) & 0x1ffffff;
    u32 il = (esr_el1 >> 25) & 0x1;
    u32 ec = (esr_el1 >> 26) & 0x3f;
    u32 iss2 = (esr_el1 >> 32) & 0x7ffffff;
    u32 res0 = (esr_el1 >> 56) & 0xff;

    print_string("spsr_el1=");
    print_hex64(spsr_el1);
    print_string("\r\n");
    print_string("elr_el1=");
    print_hex64(elr_el1);
    print_string("\r\n");
    print_string("esr_el1.iss=");
    print_hex64(iss);
    print_string("\r\n");
    print_string("esr_el1.il=");
    print_hex64(il);
    print_string("\r\n");
    print_string("esr_el1.ec=");
    print_hex64(ec);
    print_string("\r\n");
    print_string("esr_el1.iss2=");
    print_hex64(iss2);
    print_string("\r\n");
    print_string("esr_el1.res0=");
    print_hex64(res0);
    print_string("\r\n");
}

void
interrupt_handler_el1_cur_irq(void)
{
    handle_irq_interrupt();
}

void
interrupt_handler_el1_low_sync(void)
{
    __asm__ volatile("msr DAIFSet, 0xf");
    u64 spsr_el1 = 0;
    u64 elr_el1 = 0;
    u64 esr_el1 = 0;

    __asm__ volatile("mrs %0, spsr_el1\n"
                     "mrs %1, elr_el1\n"
                     "mrs %2, esr_el1\n"
                     : "=r"(spsr_el1), "=r"(elr_el1), "=r"(esr_el1));

    print_string("WW\r\n");
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
interrupt_handler_el1_low_irq(void)
{
    handle_irq_interrupt();
}

void
interrupt_handler_default(void)
{
    print_string("unknown interrupt\r\n");
}
