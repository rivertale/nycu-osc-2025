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
    
    void *result = (void *)((u8 *)header + sizeof(*header) +
                            align2_up(name_size, 2) + align2_up(file_size, 4));
    if(!cpio_is_valid(result) || cpio_is_sentinel(result))
        result = 0;
    
    return result;
}

static void *
cpio_find_first_file(void)
{
    void *handle = g_cpio_base;
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
    
    u8 *result = (u8 *)header + sizeof(*header) + align2_up(name_size, 2);
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
