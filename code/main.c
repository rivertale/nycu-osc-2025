#include <stdint.h>

// Mini UART
#define AUX_IRQ 0x3f215000
#define AUX_ENABLES 0x3f215004
#define AUX_MU_IO_REG 0x3f215040
#define AUX_MU_IER_REG 0x3f215044
#define AUX_MU_IIR_REG 0x3f215048
#define AUX_MU_LCR_REG 0x3f21504c
#define AUX_MU_MCR_REG 0x3f215050
#define AUX_MU_LSR_REG 0x3f215054
#define AUX_MU_MSR_REG 0x3f215058
#define AUX_MU_SCRATCH 0x3f21505c
#define AUX_MU_CNTL_REG 0x3f215060
#define AUX_MU_STAT_REG 0x3f215064
#define AUX_MU_BAUD_REG 0x3f215068
#define AUX_SPI0_CNTL0_REG 0x3f215080
#define AUX_SPI0_CNTL1_REG 0x3f215084
#define AUX_SPI0_STAT_REG 0x3f215088
#define AUX_SPI0_IO_REG 0x3f215090
#define AUX_SPI0_PEEK_REG 0x3f215094
#define AUX_SPI1_CNTL0_REG 0x3f2150c0
#define AUX_SPI1_CNTL1_REG 0x3f2150c4
#define AUX_SPI1_STAT_REG 0x3f2150c8
#define AUX_SPI1_IO_REG 0x3f2150d0
#define AUX_SPI1_PEEK_REG 0x3f2150d4

#define GPFSEL0 0x3f200000
#define GPFSEL1 0x3f200004
#define GPFSEL2 0x3f200008
#define GPFSEL3 0x3f20000c
#define GPFSEL4 0x3f200010
#define GPFSEL5 0x3f200014

#define GPPUD 0x3f200094
#define GPPUDCLK0 0x3f200098
#define GPPUDCLK1 0x3f20009C

#define GPFSEL_ALT0 4
#define GPFSEL_ALT1 5
#define GPFSEL_ALT2 6
#define GPFSEL_ALT3 7
#define GPFSEL_ALT4 3
#define GPFSEL_ALT5 2

// Mailbox
#define MAILBOX_READ 0x3f00b880
#define MAILBOX_STATUS 0x3f00b898
#define MAILBOX_WRITE 0x3f00b8a0

#define MAILBOX_EMPTY 0x40000000
#define MAILBOX_FULL 0x80000000

#define MAILBOX_REQUEST 0x00000000
#define MAILBOX_RESPONSE_SUCCESS 0x80000000
#define MAILBOX_RESPONSE_ERROR 0x80000001

#define MAILBOX_TAG_END 0x00000000
#define MAILBOX_TAG_REQUEST 0x00000000
#define MAILBOX_TAG_GET_BOARD_REVISION 0x00010002
#define MAILBOX_TAG_GET_ARM_MEMORY 0x00010005

#define do_nothing
#define array_count(array) (sizeof(array) / sizeof(array[0]))
#define wait_cycle(delay) for(uint32_t i = 0; i < delay; ++i) __asm__ volatile("nop")

typedef struct ArmMemoryInfo
{
    uint32_t base;
    uint32_t size;
} __attribute__((aligned(8))) ArmMemoryInfo;

static uint32_t
string_len(char *string)
{
    uint32_t result = 0;
    while(*string++)
        ++result;
    return result;
}

static int
string_match(char *a, char *b)
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
trim_crlf(char *string)
{
    uint32_t len = string_len(string);
    while(len > 0)
    {
        char c = string[len - 1];
        if(c != '\r' && c != '\n')
            break;
        string[--len] = '\0';
    }
}

static char
mini_uart_read_char(void)
{
    while(!((*(volatile uint32_t *)AUX_MU_LSR_REG) & 0x1))
        do_nothing;
    
    char result = *(volatile uint32_t *)AUX_MU_IO_REG;
    return result;
}

static void
mini_uart_write_char(char c)
{
    while(!((*(volatile uint32_t *)AUX_MU_LSR_REG) & 0x20))
        do_nothing;
    
    *(volatile uint32_t *)AUX_MU_IO_REG = c;
}

static void
mini_uart_init(void)
{
    *(volatile uint32_t *)GPFSEL1 &= ~((7 << 12) | (7 << 15));
    *(volatile uint32_t *)GPFSEL1 |= (2 << 12) | (2 << 15);
    
    *(volatile uint32_t *)GPPUD = 0;
    wait_cycle(150);
    *(volatile uint32_t *)GPPUDCLK0 = (1 << 14) | (1 << 15);
    wait_cycle(150);
    *(volatile uint32_t *)GPPUD = 0;
    *(volatile uint32_t *)GPPUDCLK0 = 0;
    
    *(volatile uint32_t *)AUX_ENABLES |= 1;
    *(volatile uint32_t *)AUX_MU_CNTL_REG = 0;
    *(volatile uint32_t *)AUX_MU_IER_REG = 0;
    *(volatile uint32_t *)AUX_MU_LCR_REG = 3;
    *(volatile uint32_t *)AUX_MU_MCR_REG = 0;
    *(volatile uint32_t *)AUX_MU_BAUD_REG = 270;
    *(volatile uint32_t *)AUX_MU_IIR_REG = 6;
    *(volatile uint32_t *)AUX_MU_CNTL_REG = 3;
}

static void
print_string(char *string)
{
    while(*string)
    {
        mini_uart_write_char(*string);
        ++string;
    }
}

static void
print_hex32(uint32_t value)
{
    static char hex_digit[16] =
    {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    
    char digits[11] =
    {
        '0',
        'x',
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
    print_string(digits);
}

static int
mailbox_query(uint32_t *message)
{
    int result = 0;
    uint32_t channel = 8;
    uint32_t channel_mask = 0xf;
    uint32_t mailbox = ((uint32_t)(uint64_t)message & ~channel_mask) | channel;
    while(*(volatile uint32_t *)MAILBOX_STATUS & MAILBOX_FULL)
        do_nothing;
    *(volatile uint32_t *)MAILBOX_WRITE = mailbox;
    
    while(*(volatile uint32_t *)MAILBOX_STATUS & MAILBOX_EMPTY)
        do_nothing;
    
    if(*(volatile uint32_t *)MAILBOX_READ == mailbox)
        result = (message[1] == MAILBOX_RESPONSE_SUCCESS);
    
    return result;
}

static uint32_t
query_board_revision(void)
{
    // reference
    // https://jsandler18.github.io/extra/prop-channel.html
    // https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
    uint32_t result = 0;
    static __attribute__((aligned(16))) uint32_t message[7];
    
    message[0] = sizeof(message);
    message[1] = MAILBOX_REQUEST;
    message[2] = MAILBOX_TAG_GET_BOARD_REVISION;
    message[3] = 4; // max(request_len=0, response_len=4)
    message[4] = MAILBOX_TAG_REQUEST;
    message[5] = 0; // response buffer
    message[6] = MAILBOX_TAG_END;
    
    if(mailbox_query(message))
        result = message[5];
    return result;
}

static ArmMemoryInfo
query_arm_memory_info(void)
{
    ArmMemoryInfo result = {0};
    __attribute__((aligned(16))) uint32_t message[8];
    message[0] = sizeof(message);
    message[1] = MAILBOX_REQUEST;
    message[2] = MAILBOX_TAG_GET_ARM_MEMORY;
    message[3] = 8; // max(request_len=0, response_len=8)
    message[4] = MAILBOX_TAG_REQUEST;
    message[5] = 0; // response buffer
    message[6] = 0; // response buffer
    message[7] = MAILBOX_TAG_END;
    
    if(mailbox_query(message))
    {
        result.base = message[5];
        result.size = message[6];
    }
    return result;
}

static void
scan_line(char *string, uint32_t max_len)
{
    if(max_len > 0)
    {
        while(max_len-- > 1)
        {
            char c = mini_uart_read_char();
            mini_uart_write_char(c);
            *string++ = c;
            if(c == '\r')
                break;
        }
        *string++ = '\0';
    }
}

int
kernel_main()
{
    mini_uart_init();
    print_string("Welcome to OSC Lab1!\r\n");
    for(;;)
    {
        char command[256];
        print_string("# ");
        scan_line(command, array_count(command));
        trim_crlf(command);
        print_string("\n");
        
        if(string_match(command, "help"))
        {
            char *usage =
                "help    : print this help menu\r\n"
                "hello   : print Hello World!\r\n"
                "mailbox : print hardware's information\r\n";
            print_string(usage);
        }
        else if(string_match(command, "hello"))
        {
            print_string("Hello World!\r\n");
        }
        else if(string_match(command, "mailbox"))
        {
            uint32_t revision = query_board_revision();
            print_string("Mailbox info:\r\n");
            print_string("Board revision: ");
            print_hex32(revision);
            print_string("\r\n");
            
            ArmMemoryInfo memory_info = query_arm_memory_info();
            print_string("ARM memory base address: ");
            print_hex32(memory_info.base);
            print_string("\r\n");
            print_string("ARM memory size: ");
            print_hex32(memory_info.size);
            print_string("\r\n");
        }
        else
        {
            print_string("Unrecognized command\r\n");
        }
    }
    return 0;
}