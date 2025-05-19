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
mini_uart_echo(void)
{
    u8 c = mini_uart_read_byte();
    mini_uart_write_byte(c);
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
mini_uart_write_u64(u64 value)
{
    c8 digits[32];

    um32 cur = array_count(digits);
    if(value > 0)
    {
        while(value > 0)
        {
            digits[--cur] = value % 10 + '0';
            value /= 10;
        }
    }
    else
    {
        digits[--cur] = '0';
    }

    for(c8 *c = digits + cur; *c; ++c)
        mini_uart_write_byte(*c);
}

static void
mini_uart_write_hex64(u64 value)
{
    static c8 hex_digit[16] =
    {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    c8 digits[] =
    {
        '0', 'x',
        hex_digit[(value >> 28) & 0xf],
        hex_digit[(value >> 24) & 0xf],
        hex_digit[(value >> 20) & 0xf],
        hex_digit[(value >> 16) & 0xf],
        hex_digit[(value >> 12) & 0xf],
        hex_digit[(value >> 8) & 0xf],
        hex_digit[(value >> 4) & 0xf],
        hex_digit[(value >> 0) & 0xf],
        '\0'
    };

    for(c8 *c = digits; *c; ++c)
        mini_uart_write_byte(*c);
}
static void
mini_uart_enable_read_interrupt(void)
{
    *(vu32 *)AUX_MU_IER_REG |= AUX_MU_IER_RECEIVE_ENABLED;
}

static void
mini_uart_disable_read_interrupt(void)
{
    *(vu32 *)AUX_MU_IER_REG &= ~AUX_MU_IER_RECEIVE_ENABLED;
}

static void
mini_uart_enable_write_interrupt(void)
{
    *(vu32 *)AUX_MU_IER_REG |= AUX_MU_IER_TRANSMIT_ENABLED;
}

static void
mini_uart_disable_write_interrupt(void)
{
    *(vu32 *)AUX_MU_IER_REG &= ~AUX_MU_IER_TRANSMIT_ENABLED;
}

static void
mini_uart_init(void)
{
    *(vu32 *)GPIO_FSEL_REG1 &= ~(GPIO_FSEL_MASK(4) | GPIO_FSEL_MASK(5));
    *(vu32 *)GPIO_FSEL_REG1 |= GPIO_FSEL_ALT5(4) | GPIO_FSEL_ALT5(5);

    *(vu32 *)GPIO_PUD_REG = GPIO_PUD_OFF;
    wait_cycle(150);
    *(vu32 *)GPIO_PUDCLK0_REG = (1 << 14) | (1 << 15);
    wait_cycle(150);
    *(vu32 *)GPIO_PUDCLK0_REG = 0;

    *(vu32 *)AUX_ENABLES = AUX_ENABLES_MINI_UART;
    *(vu32 *)AUX_MU_CNTL_REG = 0;
    *(vu32 *)AUX_MU_IER_REG = 0;
    *(vu32 *)AUX_MU_LCR_REG = 3;
    *(vu32 *)AUX_MU_MCR_REG = 0;
    *(vu32 *)AUX_MU_BAUD_REG = 270;
    *(vu32 *)AUX_MU_IIR_REG = AUX_MU_IIR_TRANSMIT | AUX_MU_IIR_RECEIVE;
    *(vu32 *)AUX_MU_CNTL_REG = 3;
}
