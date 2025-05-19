#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

typedef enum Syscall
{
    Syscall_get_process_id = 0,
    Syscall_read_console = 1,
    Syscall_write_console = 2,
    Syscall_exec = 3,
    Syscall_fork = 4,
    Syscall_exit_current_process = 5,
    Syscall_mailbox_query = 6,
    Syscall_send_kill_signal = 7,
    Syscall_set_signal_handler = 8,
    Syscall_send_signal = 9,
    Syscall_alloc_memory = 10,
    Syscall_exit_signal_handler = 11,
    Syscall_exit_current_thread = 12,
} Syscall;

#endif //KERNEL_SYSCALL_H
