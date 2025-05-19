// functions in this file are wrappers for user-space program, they should live in a library a user
// program always link to. since the program know nothing about our library, we place these
// functions in kernel and treat them as if they are part of the user program

// wrapper for main(), bridging userdata to arg_count and args.
static
THREAD_PROC(__attribute__((section(".text.bridge"))) bridge_process_startup)
{
    ProcessStartup *startup = (ProcessStartup *)userdata;
    ExitCode exit_code = startup->proc(startup->arg_count, startup->args);
    return exit_code;
}

static void __attribute__((naked, section(".text.bridge")))
bridge_thread_cleanup(ExitCode exit_code)
{
    __asm__ volatile("mov x8, %0\n"
                     "svc 0\n"
                     "ret\n"
                     :: "i"(Syscall_exit_current_thread));
}

static void __attribute__((section(".text.bridge")))
bridge_signal_handler_cleanup(void)
{
    __asm__ volatile("mov x8, %0\n"
                     "svc 0\n"
                     "ret\n"
                     :: "i"(Syscall_exit_signal_handler));
}
