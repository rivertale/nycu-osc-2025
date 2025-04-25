#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"
#include "peripheral.h"
#include "kernel_device.h"
#include "uart.h"
#include "kernel_memory.h"
#include "kernel_scheduler.h"

// TODO: the naming is starting to be confusing between kernel and user program
#define USER_SPACE_BEGIN 0x800000
#define USER_SPACE_END 0xb00000

#define double_link_insert_at_last(sentinel, link) \
do \
{ \
(link)->prev = (sentinel)->prev; \
(link)->next = (sentinel); \
(sentinel)->prev->next = (link); \
(sentinel)->prev = (link); \
} while(0)

#define double_link_remove(link) \
do \
{ \
(link)->prev->next = (link)->next; \
(link)->next->prev = (link)->prev; \
} while(0)

#define double_link_init(sentinel) \
do \
{ \
(sentinel)->prev = (sentinel); \
(sentinel)->next = (sentinel); \
} while(0)

#define double_link_is_empty(link) ((link)->next == (link))

#define KERNEL_MAX_IO_BUFFER_SIZE 4096
#define KERNEL_IO_BUFFER_MASK (KERNEL_MAX_IO_BUFFER_SIZE - 1)
#define KERNEL_MAX_TIMER 1024
#define KERNEL_MAX_INTERRUPT 1024

#define KERNEL_TIMER_CALLBACK(name) void name(void *userdata)
typedef KERNEL_TIMER_CALLBACK(KernelTimerCallback);

typedef struct Timer
{
    u32 next_free;
    u32 next_expired;
    u64 tick;
    KernelTimerCallback *callback;
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
    ThreadId prev_created_thread_id;
    
    PagePool page_pool;
    MemoryAllocator allocator;
    Scheduler scheduler;
    
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
