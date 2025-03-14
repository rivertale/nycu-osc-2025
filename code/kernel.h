#ifndef KERNEL_H
#define KERNEL_H

#include "common.h"
#include "devicetree.h"
#include "watchdog.h"
#include "uart.h"
#include "mailbox.h"
#include "cpio.h"

#define HEAP_MIN_ORDER 4 // 16 byte allocation - at least hold a HeapFreeLink
#define HEAP_MAX_ORDER 18 // 1mb allocation
#define HEAP_ORDER_BIT_IN_MAP 4 // we assumed it's divided by 64
#define HEAP_MIN_ALLOCATION (1 << HEAP_MIN_ORDER)
#define HEAP_MAX_ALLOCATION (1 << HEAP_MAX_ORDER)

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
} Heap;

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

extern void *heap_begin;
extern void *heap_size;

#endif //KERNEL_H
