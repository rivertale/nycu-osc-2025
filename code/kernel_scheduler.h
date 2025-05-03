#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H

#define USER_THREAD_DEFAULT_PSTATE 0x340
#define KERNEL_THREAD_DEFAULT_PSTATE 0x345
#define THREAD_PROC(name) ExitCode name(void *userdata)

typedef enum ThreadStatus
{
    ThreadStatus_none,
    ThreadStatus_exited,
} ThreadStatus;

typedef enum CreateThreadFlag
{
    CreateThread_kernel = 0x1,
} CreateThreadFlag;

typedef u32 ThreadId;
typedef s64 ExitCode;
typedef THREAD_PROC(ThreadProc);

typedef struct TrapFrame TrapFrame;
typedef struct Process Process;

typedef enum ThreadPriority
{
    ThreadPriority_idle,
    ThreadPriority_normal,
    ThreadPriority_one_past_last,
} ThreadPriority;

typedef struct SwitchFrame
{
    u64 x19;
    u64 x20;
    u64 x21;
    u64 x22;
    u64 x23;
    u64 x24;
    u64 x25;
    u64 x26;
    u64 x27;
    u64 x28;
    u64 fp;
    u64 lr;
    u64 tpidr_el1;
    u64 reserved;
} SwitchFrame;

typedef struct Thread
{
    Link schedule_link;
    Link thread_link;

    ThreadId id;
    ThreadStatus status;
    Process *process;
    ThreadPriority priority;
    u64 kernel_sp;

    um32 user_stack_size;
    um32 kernel_stack_size;
    u64 user_stack_addr;
    u64 kernel_stack_addr;
} Thread;

typedef struct Scheduler
{
    Link ready_link[ThreadPriority_one_past_last];
} Scheduler;

#endif //KERNEL_SCHEDULER_H
