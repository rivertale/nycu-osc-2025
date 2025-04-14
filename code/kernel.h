#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"
#include "peripheral.h"
#include "kernel_cpio.h"
#include "kernel_devicetree.h"
#include "kernel_mailbox.h"
#include "kernel_memory.h"
#include "kernel_watchdog.h"
#include "uart.h"

#define CORE0_TIMER_IRQ_CTRL 0x40000040

#define HEAP_MIN_ORDER 4 // 16 byte allocation - at least hold a HeapFreeLink
#define HEAP_MAX_ORDER 18 // 1mb allocation
#define HEAP_ORDER_BIT_IN_MAP 4 // we assumed it's divided by 64
#define HEAP_MIN_ALLOCATION (1 << HEAP_MIN_ORDER)
#define HEAP_MAX_ALLOCATION (1 << HEAP_MAX_ORDER)

// TODO: the naming is starting to be confusing between kernel and user program
#define USER_SPACE_BEGIN 0x800000
#define USER_SPACE_END 0xb00000
extern void *heap_begin;
extern void *heap_size;

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

typedef struct HeapBlock
{
    struct HeapBlock *prev;
    struct HeapBlock *next;
} HeapBlock;

typedef struct Heap
{
    u32 served_mask;
    s32 order_shift;
    umm in_used;
    umm total;
    u8 *base;
    u64 *served;
    HeapBlock free_block[HEAP_MAX_ORDER + 1];
    u8 *memory;
    umm memory_used;
} Heap;

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
    u32 read_cur0;
    u32 read_cur1;
    u32 write_cur0;
    u32 write_cur1;

    u32 first_free_timer;
    u32 first_expired_timer;

    u32 first_free_interrupt;
    u32 first_prioritized_interrupt;

    void *devicetree_begin;
    void *devicetree_end;
    union { void *cpio_begin; void *first_device_addr; };
    union { void *cpio_end; void *last_device_addr; };

    u8 read_buffer[KERNEL_MAX_IO_BUFFER_SIZE];
    u8 write_buffer[KERNEL_MAX_IO_BUFFER_SIZE];
    Timer timers[KERNEL_MAX_TIMER];
    InterruptContext interrupts[KERNEL_MAX_INTERRUPT];
} KernelState;

extern void *kernel_image_begin;
extern void *kernel_image_end;

static KernelState g_kernel_state = {0};

#endif //KERNEL_H
