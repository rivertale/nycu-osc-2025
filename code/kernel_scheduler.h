#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H

#define MAX_SCHEDULE_PRIORITY 8

typedef enum SavedReg
{
    SavedReg_tpidr_el1,
    SavedReg_sp,
    SavedReg_x9,
    SavedReg_x10,
    SavedReg_x19,
    SavedReg_x20,
    SavedReg_x21,
    SavedReg_x22,
    SavedReg_x23,
    SavedReg_x24,
    SavedReg_x25,
    SavedReg_x26,
    SavedReg_x27,
    SavedReg_x28,
    SavedReg_fp,
    SavedReg_lr,
    SavedReg_one_past_last
} SavedReg;

typedef u32 ThreadId;

typedef struct ThreadLink
{
    struct ThreadLink *prev;
    struct ThreadLink *next;
} ThreadLink;

typedef struct Thread
{
    ThreadLink link;
    
    u64 saved_regs[SavedReg_one_past_last];
    ThreadId id;
    s32 priority;
} Thread;

typedef struct Scheduler
{
    ThreadLink ready_link[MAX_SCHEDULE_PRIORITY];
} Scheduler;

#endif //KERNEL_SCHEDULER_H
