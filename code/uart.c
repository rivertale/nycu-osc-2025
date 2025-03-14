#define mini_uart_write_const(array) \
    mini_uart_write(array, (array_count(array) - 1) * sizeof(array[0]))

static u8
mini_uart_read_byte(void)
{
    while(!(*(vu32 *)AUX_MU_LSR_REG & 0x1))
        do_nothing;
    
    u8 result = *(vu32 *)AUX_MU_IO_REG;
    return result;
}

static void
mini_uart_write_byte(u8 byte)
{
    while(!(*(vu32 *)AUX_MU_LSR_REG & 0x20))
        do_nothing;
    
    *(vu32 *)AUX_MU_IO_REG = byte;
}

static void
mini_uart_read(void *buffer, umm size)
{
    u8 *cur = (u8 *)buffer;
    while(size-- > 0)
        *cur++ = mini_uart_read_byte();
}

static void
mini_uart_write(void *buffer, umm size)
{
    u8 *cur = (u8 *)buffer;
    while(size-- > 0)
        mini_uart_write_byte(*cur++);
}

static void
mini_uart_init(void)
{
    *(vu32 *)GPFSEL1 &= ~((GPFSEL_ALT_MASK << 12) | (GPFSEL_ALT_MASK << 15));
    *(vu32 *)GPFSEL1 |= (GPFSEL_ALT5 << 12) | (GPFSEL_ALT5 << 15);
    
    *(vu32 *)GPPUD = 0;
    wait_cycle(150);
    *(vu32 *)GPPUDCLK0 = (1 << 14) | (1 << 15);
    wait_cycle(150);
    *(vu32 *)GPPUD = 0;
    *(vu32 *)GPPUDCLK0 = 0;
    
    *(vu32 *)AUX_ENABLES = 1;
    *(vu32 *)AUX_MU_CNTL_REG = 0;
    *(vu32 *)AUX_MU_IER_REG = 0;
    *(vu32 *)AUX_MU_LCR_REG = 3;
    *(vu32 *)AUX_MU_MCR_REG = 0;
    *(vu32 *)AUX_MU_BAUD_REG = 270;
    *(vu32 *)AUX_MU_IIR_REG = 6;
    *(vu32 *)AUX_MU_CNTL_REG = 3;
}
