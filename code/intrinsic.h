#ifndef INTRINSIC_H
#define INTRINSIC_H

static inline u64
read_cntfrq_el0(void)
{
    u64 result;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(result));
    return result;
}

static inline void
write_cntfrq_el0(u64 value)
{
    __asm__ volatile("msr cntfrq_el0, %0" :: "r"(value));
}

static inline u64
read_cntkctl_el1(void)
{
    u64 result;
    __asm__ volatile("mrs %0, cntkctl_el1" : "=r"(result));
    return result;
}

static inline void
write_cntkctl_el1(u64 value)
{
    __asm__ volatile("msr cntkctl_el1, %0" :: "r"(value));
}

static inline u64
read_cntpct_el0(void)
{
    u64 result;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(result));
    return result;
}

static inline void
write_cntpct_el0(u64 value)
{
    __asm__ volatile("msr cntpct_el0, %0" :: "r"(value));
}

static inline u64
read_cntp_ctl_el0(void)
{
    u64 result;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(result));
    return result;
}

static inline void
write_cntp_ctl_el0(u64 value)
{
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(value));
}

static inline u64
read_cntp_cval_el0(void)
{
    u64 result;
    __asm__ volatile("mrs %0, cntp_cval_el0" : "=r"(result));
    return result;
}

static inline void
write_cntp_cval_el0(u64 value)
{
    __asm__ volatile("msr cntp_cval_el0, %0" :: "r"(value));
}

static inline u64
read_cpacr_el1(void)
{
    u64 result;
    __asm__ volatile("mrs %0, cpacr_el1" : "=r"(result));
    return result;
}

static inline void
write_cpacr_el1(u64 value)
{
    __asm__ volatile("msr cpacr_el1, %0" :: "r"(value));
}

static inline void
enable_irq_interrupt(void)
{
    __asm__ volatile("msr daifclr, 0x2");
}

static inline void
disable_irq_interrupt(void)
{
    __asm__ volatile("msr daifset, 0x2");
}

static inline u64
read_elr_el1(void)
{
    u64 result;
    __asm__ volatile("mrs %0, elr_el1" : "=r"(result));
    return result;
}

static inline void
write_elr_el1(u64 value)
{
    __asm__ volatile("msr elr_el1, %0" :: "r"(value));
}

static inline u64
read_esr_el1(void)
{
    u64 result;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(result));
    return result;
}

static inline void
write_esr_el1(u64 value)
{
    __asm__ volatile("msr esr_el1, %0" :: "r"(value));
}

static inline u64
read_spsr_el1(void)
{
    u64 result;
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(result));
    return result;
}

static inline void
write_spsr_el1(u64 value)
{
    __asm__ volatile("msr spsr_el1, %0" :: "r"(value));
}

static inline u64
read_tpidr_el1(void)
{
    u64 result;
    __asm__ volatile("mrs %0, tpidr_el1" : "=r"(result));
    return result;
}

static inline void
write_tpidr_el1(u64 value)
{
    __asm__ volatile("msr tpidr_el1, %0" :: "r"(value));
}

static void
wait_cycle(u64 delay)
{
    __asm__ volatile("1:"
                     "subs %0, %0, #1\n"
                     "bne 1b\n"
                     : "+r"(delay));
}

static inline s32
count_leading_zeros(u64 value)
{
    s64 result = 0;
    __asm__ volatile("clz %0, %1" : "=r"(result) : "r"(value));
    return result;
}

static inline s32
count_trailing_zeros(u64 value)
{
    s64 result = 0;
    __asm__ volatile("rbit %0, %0\n"
                     "clz %1, %0\n"
                     : "=r"(result) : "r"(value));
    return result;
}

static inline s32
find_most_significant_bit(u64 value)
{
    return 63 - count_leading_zeros(value);
}

static inline s32
find_least_significant_bit(u64 value)
{
    return count_trailing_zeros(value);
}

#endif // INTRINSIC_H
