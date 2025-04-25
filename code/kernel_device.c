static void
watchdog_reboot(s32 tick)
{
    *(vu32 *)PM_RSTC = PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
    *(vu32 *)PM_WDOG = PM_PASSWORD | tick;
}

static void
watchdog_cancel_reboot(void)
{
    *(vu32 *)PM_RSTC = PM_PASSWORD | 0;
    *(vu32 *)PM_WDOG = PM_PASSWORD | 0;
}
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
    __attribute__((aligned(16))) u32 message[8];
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

static u32
devicetree_get_total_size(void *handle)
{
    FdtHeader *header = (FdtHeader *)handle;
    return devicetree_u32(&header->totalsize);
}

static void
advance_devicetree_iter(DevicetreeIter *iter)
{
    for(;;)
    {
        u32 tag = devicetree_u32(iter->cur);
        iter->cur += sizeof(u32);
        
        if(tag == FDT_BEGIN_NODE)
        {
            c8 *name = (c8 *)iter->cur;
            um32 name_len = string_len(name);
            
            assert(iter->dir_len + name_len + 1 < DEVICETREE_MAX_DIR_LEN);
            copy_memory(iter->dir + iter->dir_len, name, name_len);
            iter->dir[iter->dir_len + name_len + 0] = '/';
            iter->dir[iter->dir_len + name_len + 1] = '\0';
            iter->dir_len += name_len + 1;
            
            ++iter->depth;
            iter->cur += align_up(name_len + 1, 4);
        }
        else if(tag == FDT_END_NODE)
        {
            assert(iter->depth > 0);
            assert(iter->dir_len > 0);
            --iter->depth;
            --iter->dir_len; // delete the tailing '/';
            while(iter->dir_len > 0 && iter->dir[iter->dir_len - 1] != '/')
                --iter->dir_len;
            iter->dir[iter->dir_len] = '\0';
        }
        else if(tag == FDT_PROP)
        {
            u32 name_offset = devicetree_u32(iter->cur + sizeof(u32));
            iter->size = devicetree_u32(iter->cur);
            iter->prop = iter->strings_block + name_offset;
            iter->data = iter->cur + sizeof(u32) * 2;
            
            iter->cur += sizeof(u32) * 2 + align_up(iter->size, 4);
            break;
        }
        else if(tag == FDT_NOP)
        {
            do_nothing;
        }
        else if(tag == FDT_END)
        {
            assert(iter->depth == 0 && iter->dir_len == 0);
            break;
        }
    }
}

static b32
is_devicetree_iter_valid(DevicetreeIter *iter)
{
    return (iter->dir_len > 0);
}

static DevicetreeIter
iterate_devicetree(void *handle)
{
    DevicetreeIter iter;
    clear_memory(&iter, sizeof(iter));
    
    FdtHeader *header = (FdtHeader *)handle;
    u32 magic = devicetree_u32(&header->magic);
    if(magic == 0xd00dfeed)
    {
        c8 *strings_block = (c8 *)header + devicetree_u32(&header->off_dt_strings);
        u8 *structure_block = (u8 *)header + devicetree_u32(&header->off_dt_struct);
        iter.cur = structure_block;
        iter.strings_block = strings_block;
        advance_devicetree_iter(&iter);
    }
    return iter;
}

static s8
cpio_parse_hex_digit(c8 digit)
{
    s32 result = -1;
    if('0' <= digit && digit <= '9')
        result = digit - '0';
    else if('A' <= digit && digit <= 'F')
        result = digit - 'A' + 10;
    else if('a' <= digit && digit <= 'f')
        result = digit - 'a' + 10;
    
    return result;
}

static s32
cpio_parse_newc_int(c8 *buffer)
{
    s32 result = -1;
    
    s8 digits[8];
    digits[0] = cpio_parse_hex_digit(buffer[0]);
    digits[1] = cpio_parse_hex_digit(buffer[1]);
    digits[2] = cpio_parse_hex_digit(buffer[2]);
    digits[3] = cpio_parse_hex_digit(buffer[3]);
    digits[4] = cpio_parse_hex_digit(buffer[4]);
    digits[5] = cpio_parse_hex_digit(buffer[5]);
    digits[6] = cpio_parse_hex_digit(buffer[6]);
    digits[7] = cpio_parse_hex_digit(buffer[7]);
    
    if((digits[0] | digits[1] | digits[2] | digits[3] |
        digits[4] | digits[5] | digits[6] | digits[7]) != -1)
    {
        result = (digits[0] << 28) | (digits[1] << 24) | (digits[2] << 20) | (digits[3] << 16) |
        (digits[4] << 12) | (digits[5] << 8) | (digits[6] << 4) | digits[7];
    }
    return result;
}

static b32
cpio_is_valid(void *handle)
{
    b32 result = 0;
    if(handle)
    {
        CpioNewcHeader *header = (CpioNewcHeader *)handle;
        result = memory_match(header->magic, CPIO_MAGIC, sizeof(header->magic));
    }
    return result;
}

static b32
cpio_is_sentinel(void *handle)
{
    b32 result = 0;
    
    CpioNewcHeader *header = (CpioNewcHeader *)handle;
    u32 sentinel_len = sizeof(CPIO_SENTINEL_FILE) - 1;
    if(cpio_parse_newc_int(header->namesize) == sizeof(CPIO_SENTINEL_FILE))
    {
        c8 *file_name = (c8 *)(header + 1);
        result = memory_match(file_name, CPIO_SENTINEL_FILE, sentinel_len);
    }
    return result;
}

static void *
cpio_next_file(void *handle)
{
    CpioNewcHeader *header = (CpioNewcHeader *)handle;
    u32 name_size = cpio_parse_newc_int(header->namesize);
    u32 file_size = cpio_parse_newc_int(header->filesize);
    
    void *result = (void *)((u8 *)header + align_up(sizeof(*header) + name_size, 4) +
                            align_up(file_size, 4));
    if(!cpio_is_valid(result) || cpio_is_sentinel(result))
        result = 0;
    
    return result;
}

static void *
cpio_find_first_file(void)
{
    void *handle = g_kernel_state.cpio_handle;
    if(!cpio_is_valid(handle) || cpio_is_sentinel(handle))
        handle = 0;
    
    return handle;
}

static c8 *
cpio_get_file_name(void *handle, um32 *out_len)
{
    CpioNewcHeader *header = (CpioNewcHeader *)handle;
    u32 name_size = cpio_parse_newc_int(header->namesize);
    
    c8 *result = (c8 *)header + sizeof(*header);
    if(out_len)
        *out_len = name_size;
    return result;
}

static u8 *
cpio_get_file_content(void *handle, um32 *out_len)
{
    CpioNewcHeader *header = (CpioNewcHeader *)handle;
    u32 name_size = cpio_parse_newc_int(header->namesize);
    u32 file_size = cpio_parse_newc_int(header->filesize);
    
    u8 *result = (u8 *)header + align_up(sizeof(*header) + name_size, 4);
    if(out_len)
        *out_len = file_size;
    return result;
}

static void *
cpio_find_file(c8 *file_name)
{
    void *result = 0;
    
    umm len = string_len(file_name);
    for(void *handle = cpio_find_first_file();
        handle;
        handle = cpio_next_file(handle))
    {
        um32 this_len;
        c8 *this_name = cpio_get_file_name(handle, &this_len);
        if(string_match(this_name, file_name))
        {
            result = handle;
            break;
        }
    }
    return result;
}

static void
query_device_region_list(DeviceRegionList *list, void *devicetree_handle)
{
    clear_memory(list, sizeof(*list));
    list->spin_table_begin = (void *)0x0000;
    list->spin_table_end = (void *)0x1000;
    list->devicetree_begin = devicetree_handle;
    list->devicetree_end = devicetree_handle + devicetree_get_total_size(devicetree_handle);
    for(DevicetreeIter iter = iterate_devicetree(devicetree_handle);
        is_devicetree_iter_valid(&iter);
        advance_devicetree_iter(&iter))
    {
        if(string_match(iter.dir, "/chosen/"))
        {
            if(string_match(iter.prop, "linux,initrd-start"))
            {
                list->cpio_begin = (void *)(umm)devicetree_u32(iter.data);
            }
            else if(string_match(iter.prop, "linux,initrd-end"))
            {
                list->cpio_end = (void *)(umm)devicetree_u32(iter.data);
            }
        }
    }
    
    // ensure every address is initialized
    for(void **addr = (void **)list; addr < (void **)(list + 1); ++addr)
    {
        if(addr != &list->spin_table_begin)
            assert(*addr);
    }
}
