#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef char c8;
typedef float f32;
typedef double f64;
typedef int b32;
typedef u32 um32;
typedef u64 umm;

typedef volatile u32 vu32;


#define do_nothing
#define array_count(array) (sizeof(array) / sizeof(array[0]))
#define assert(condition) if(!(condition)) __asm__ volatile ("brk #0")
#define wait_cycle(delay) for(uint32_t i = 0; i < delay; ++i) __asm__ volatile("nop")

static u64
next_power_of_two(u64 value)
{
    --value;
    value |= (value >> 1);
    value |= (value >> 2);
    value |= (value >> 4);
    value |= (value >> 8);
    value |= (value >> 16);
    value |= (value >> 32);
    return value + 1;
}

static u64
align2_up(u64 value, s32 alignment)
{
    u64 mask = alignment - 1;
    return (value + mask) & ~mask;
}

static void
clear_memory(void *ptr, umm size)
{
    u8 *cur = (u8 *)ptr;
    while(size-- > 0)
        *cur++ = 0;
}

static b32
memory_match(void *a, void *b, umm size)
{
    b32 result = 1;
    
    u8 *byte_a = (u8 *)a;
    u8 *byte_b = (u8 *)b;
    while(size-- > 0)
    {
        if(*byte_a++ != *byte_b++)
            return 0;
    }
    return 1;
}


static umm
string_len(c8 *string)
{
    umm result = 0;
    while(*string++)
        ++result;
    
    return result;
}

static b32
string_match(c8 *a, c8 *b)
{
    while(*a == *b)
    {
        if(*a == 0)
            return 1;
        ++a;
        ++b;
    }
    return 0;
}

static void
copy_memory(void *to, void *from, umm size)
{
    u8 *from_byte = (u8 *)from;
    u8 *to_byte = (u8 *)to;
    while(size-- > 0)
        *to_byte++ = *from_byte++;
}

#endif //COMMON_H
