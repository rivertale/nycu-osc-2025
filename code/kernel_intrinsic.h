#ifndef KERNEL_INTRINSIC_H
#define KERNEL_INTRINSIC_H

__attribute__((always_inline)) static Thread *
get_current_thread(void)
{
    Thread *result;
    __asm__ volatile("mrs %0, tpidr_el1" : "=r"(result));
    return result;
}

#endif //KERNEL_INTRINSIC_H

