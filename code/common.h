#ifndef COMMON_H
#define COMMON_H

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef char c8;
typedef float f32;
typedef double f64;
typedef int b32;
typedef u32 um32;
typedef u64 umm;

typedef volatile u32 vu32;

#define do_nothing (void)0
#define array_count(array) (sizeof(array) / sizeof(array[0]))
#define wait_cycle(delay) for(u32 i = 0; i < delay; ++i) __asm__ volatile("nop")
#define kilobytes(value) ((value) << 10)
#define megabytes(value) (kilobytes(value) << 10)
#define gigabytes(value) ((u64)megabytes(value) << 10)

#define debug_log(log) debug_log1(log, __FILE__, __LINE__)
#define debug_log1(log, file, line) debug_log2(log, file, #line)
#define debug_log2(log, file, line) mini_uart_write_const("[DEBUG] " file "(" line "): " log);

#define mini_uart_write_const(array) \
mini_uart_write(array, (array_count(array) - 1) * sizeof(array[0]))
static void mini_uart_write(void *buffer, umm size);

#define invalid_code_path assert(0)
#define assert(condition) assert1(condition, __FILE__, __LINE__)
#define assert1(condition, file, line) assert2(condition, file, line)
#define assert2(condition, file, line) \
do \
{ \
    if(!(condition)) \
    { \
        mini_uart_write_const(file "(" #line "): assertion failed\r\n"); \
        for(;;) \
            do_nothing; \
    } \
} while(0)

static s32
count_leading_zeros(u64 value)
{
    s64 result = 0;
    __asm__ volatile ("clz %0, %1" : "=r"(result) : "r"(value));
    return result;
}

static s32
count_trailing_zeros(u64 value)
{
    s64 result = 0;
    __asm__ volatile ("rbit %0, %0\n"
                      "clz %1, %0\n"
                      : "=r"(result) : "r"(value));
    return result;
}

static s32
find_most_significant_bit(u64 value)
{
    return 63 - count_leading_zeros(value);
}

static s32
find_least_significant_bit(u64 value)
{
    return count_trailing_zeros(value);
}

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
align_up(u64 value, u64 alignment)
{
    u64 mask = alignment - 1;
    return (value + mask) & ~mask;
}

static u64
align_down(u64 value, u64 alignment)
{
    u64 mask = alignment - 1;
    return value & ~mask;
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
