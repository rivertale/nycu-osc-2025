static void
copy_from_signal_frame(TrapFrame *trap_frame, SignalFrame *signal_frame)
{
    trap_frame->x19 = signal_frame->x19;
    trap_frame->x20 = signal_frame->x20;
    trap_frame->x21 = signal_frame->x21;
    trap_frame->x22 = signal_frame->x22;
    trap_frame->x23 = signal_frame->x23;
    trap_frame->x24 = signal_frame->x24;
    trap_frame->x25 = signal_frame->x25;
    trap_frame->x26 = signal_frame->x26;
    trap_frame->x27 = signal_frame->x27;
    trap_frame->x28 = signal_frame->x28;
    trap_frame->fp = signal_frame->fp;
    trap_frame->lr = signal_frame->lr;
    trap_frame->sp_el0 = signal_frame->sp;
    trap_frame->elr_el1 = signal_frame->pc;
    trap_frame->x0 = signal_frame->x0;
    trap_frame->x1 = signal_frame->x1;
    trap_frame->x2 = signal_frame->x2;
    trap_frame->x3 = signal_frame->x3;
    trap_frame->x4 = signal_frame->x4;
    trap_frame->x5 = signal_frame->x5;
    trap_frame->x6 = signal_frame->x6;
    trap_frame->x7 = signal_frame->x7;
    trap_frame->x8 = signal_frame->x8;
    trap_frame->x9 = signal_frame->x9;
    trap_frame->x10 = signal_frame->x10;
    trap_frame->x11 = signal_frame->x11;
    trap_frame->x12 = signal_frame->x12;
    trap_frame->x13 = signal_frame->x13;
    trap_frame->x14 = signal_frame->x14;
    trap_frame->x15 = signal_frame->x15;
    trap_frame->x16 = signal_frame->x16;
    trap_frame->x17 = signal_frame->x17;
    trap_frame->x18 = signal_frame->x18;
}

static void
copy_to_signal_frame(SignalFrame *signal_frame, TrapFrame *trap_frame)
{
    signal_frame->x19 = trap_frame->x19;
    signal_frame->x20 = trap_frame->x20;
    signal_frame->x21 = trap_frame->x21;
    signal_frame->x22 = trap_frame->x22;
    signal_frame->x23 = trap_frame->x23;
    signal_frame->x24 = trap_frame->x24;
    signal_frame->x25 = trap_frame->x25;
    signal_frame->x26 = trap_frame->x26;
    signal_frame->x27 = trap_frame->x27;
    signal_frame->x28 = trap_frame->x28;
    signal_frame->fp = trap_frame->fp;
    signal_frame->lr = trap_frame->lr;
    signal_frame->sp = trap_frame->sp_el0;
    signal_frame->pc = trap_frame->elr_el1;
    signal_frame->x0 = trap_frame->x0;
    signal_frame->x1 = trap_frame->x1;
    signal_frame->x2 = trap_frame->x2;
    signal_frame->x3 = trap_frame->x3;
    signal_frame->x4 = trap_frame->x4;
    signal_frame->x5 = trap_frame->x5;
    signal_frame->x6 = trap_frame->x6;
    signal_frame->x7 = trap_frame->x7;
    signal_frame->x8 = trap_frame->x8;
    signal_frame->x9 = trap_frame->x9;
    signal_frame->x10 = trap_frame->x10;
    signal_frame->x11 = trap_frame->x11;
    signal_frame->x12 = trap_frame->x12;
    signal_frame->x13 = trap_frame->x13;
    signal_frame->x14 = trap_frame->x14;
    signal_frame->x15 = trap_frame->x15;
    signal_frame->x16 = trap_frame->x16;
    signal_frame->x17 = trap_frame->x17;
    signal_frame->x18 = trap_frame->x18;
}

void
handle_signal(TrapFrame *trap_frame)
{
    Thread *thread = get_current_thread();
    Process *process = thread->process;

    if(process->pending_signal_cur0 != process->pending_signal_cur1)
    {
        Signal signal = process->pending_signals[process->pending_signal_cur0];
        if(process->signal_handlers[signal])
        {
            process->pending_signal_cur0 = (process->pending_signal_cur0 + 1) &
                PENDING_SIGNAL_BUFFER_MASK;

            SignalFrame *signal_frame = (SignalFrame *)(trap_frame->sp_el0 - sizeof(SignalFrame));
            copy_to_signal_frame(signal_frame, trap_frame);
            trap_frame->lr = (umm)user_space_signal_handler_cleanup;
            // NOTE: signal handler is in user mode
            trap_frame->spsr_el1 = USER_THREAD_DEFAULT_PSTATE;
            trap_frame->elr_el1 = (umm)process->signal_handlers[signal];
            trap_frame->sp_el0 = (umm)signal_frame;
            trap_frame->x0 = signal;
        }
        else
        {
            exit_thread(thread, signal);
        }
    }
}
