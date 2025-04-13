static b32
mailbox_query(u32 *message)
{
    b32 result = 0;
    u32 channel = 8;
    u32 channel_mask = 0xf;
    u32 mailbox = ((u32)(umm)message & ~channel_mask) | channel;
    while(*(vu32 *)MAILBOX_STATUS & MAILBOX_FULL)
        do_nothing;
    *(vu32 *)MAILBOX_WRITE = mailbox;
    
    while(*(vu32 *)MAILBOX_STATUS & MAILBOX_EMPTY)
        do_nothing;
    
    if(*(vu32 *)MAILBOX_READ == mailbox)
        result = (message[1] == MAILBOX_RESPONSE_SUCCESS);
    
    return result;
}

static u32
query_board_revision(void)
{
    // reference
    // https://jsandler18.github.io/extra/prop-channel.html
    // https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
    
    u32 result = 0;
    __attribute__((aligned(16))) u32 message[7];
    message[0] = sizeof(message);
    message[1] = MAILBOX_REQUEST;
    message[2] = MAILBOX_TAG_GET_BOARD_REVISION;
    message[3] = 4; // 0 (request_len) + 4 (response_len)
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
    __attribute__((aligned(16))) u32 message[8];
    message[0] = sizeof(message);
    message[1] = MAILBOX_REQUEST;
    message[2] = MAILBOX_TAG_GET_ARM_MEMORY;
    message[3] = 8; // 0 (request_len) + 8 (response_len)
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
