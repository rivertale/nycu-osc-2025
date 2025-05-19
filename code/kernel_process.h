#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#define ONE_PAST_MAX_PENDING_SIGNAL 1024
#define PENDING_SIGNAL_BUFFER_MASK (ONE_PAST_MAX_PENDING_SIGNAL - 1)
#define SIGNAL_HANDLER(name) void name(Signal signal)
#define PROCESS_PROC(name) ExitCode name(s32 arg_count, c8 **args)

typedef enum Signal
{
    Signal_none = 0,
    Signal_kill = 9,
    Signal_one_past_last = 32,
} Signal;

typedef SIGNAL_HANDLER(SignalHandler);
typedef PROCESS_PROC(ProcessProc);
typedef u32 ProcessId;

// TODO: less dynamic allocation
typedef struct ProcessStartup
{
    ProcessProc *proc;
    s32 arg_count;
    c8 **args;
} ProcessStartup;

typedef struct Process
{
    Link process_link;
    Link thread_link;
    ProcessId id;
    
    VirtualMemoryTree memory_tree;
    u64 *page_table;

    umm image_addr;
    umm image_size;
    umm bridge_addr;
    ProcessStartup *startup;

    u16 pending_signal_cur0;
    u16 pending_signal_cur1;
    Signal pending_signals[ONE_PAST_MAX_PENDING_SIGNAL];
    SignalHandler *signal_handlers[Signal_one_past_last];
} Process;

#endif //KERNEL_PROCESS_H
