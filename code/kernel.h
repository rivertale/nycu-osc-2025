#ifndef KERNEL_H
#define KERNEL_H

#define USER_SPACE_OFFSET 0x0000000000000000ull
#define KERNEL_SPACE_OFFSET 0xffff000000000000ull

#include "common.h"
#include "intrinsic.h"
#include "peripheral.h"
#include "kernel_device.h"
#include "kernel_memory.h"

#define TIMER_CALLBACK(name) void name(void *userdata)
typedef TIMER_CALLBACK(TimerCallback);

#include "kernel_scheduler.h"
#include "kernel_process.h"
#include "kernel_signal.h"
#include "kernel_syscall.h"
#include "kernel_interrupt.h"

#define KERNEL_MAX_IO_BUFFER_SIZE 4096
#define KERNEL_IO_BUFFER_MASK (KERNEL_MAX_IO_BUFFER_SIZE - 1)
#define KERNEL_MAX_TIMER 1024
#define KERNEL_MAX_INTERRUPT 1024

typedef struct Timer
{
    u32 next_free;
    u32 next_expired;
    u64 tick;
    TimerCallback *callback;
    void *userdata;
} Timer;

typedef struct PrintStringTask
{
    MemoryAllocator *allocator;
    c8 *string;
} PrintStringTask;

typedef enum InterruptKind
{
    Interrupt_timer,
    Interrupt_uart_read,
    Interrupt_uart_write,
} InterruptKind;

typedef struct InterruptContext
{
    u32 next_free;
    u32 next_prioritized;
    u32 kind;
    s32 priority;
} InterruptContext;

typedef struct KernelState
{
    ProcessId prev_created_process_id;
    ThreadId prev_created_thread_id;

    PagePool page_pool;
    MemoryAllocator allocator;
    Scheduler scheduler;

    Link process_link;

    u32 read_cur0;
    u32 read_cur1;
    u32 write_cur0;
    u32 write_cur1;

    u32 first_free_timer;
    u32 first_expired_timer;

    u32 first_free_interrupt;
    u32 first_prioritized_interrupt;

    void *cpio_handle;

    u8 read_buffer[KERNEL_MAX_IO_BUFFER_SIZE];
    u8 write_buffer[KERNEL_MAX_IO_BUFFER_SIZE];
    Timer timers[KERNEL_MAX_TIMER];
    InterruptContext interrupts[KERNEL_MAX_INTERRUPT];
} KernelState;

extern void *kernel_image_begin;
extern void *kernel_image_end;

static KernelState g_kernel_state = {0};

#endif //KERNEL_H
