#define mini_uart_write_const(array) \
mini_uart_write(array, (array_count(array) - 1) * sizeof(array[0]))
static void mini_uart_write_byte(u8 byte);
static void mini_uart_write(void *buffer, umm size);

static void
devicetree_traverse(void *devicetree_addr, DevicetreeCallback *callback, void *userdata)
{
    FdtHeader *header = (FdtHeader *)devicetree_addr;
    if(devicetree_u32(&header->magic) == 0xd00dfeed)
        do_nothing; // TODO: check magic
    
    u32 dir_depth = 0;
    u32 path_len = 0;
    c8 path[DEVICETREE_MAX_PATH_LEN];
    
    c8 *strings_block = (c8 *)header + devicetree_u32(&header->off_dt_strings);
    u8 *structure_block = (u8 *)header + devicetree_u32(&header->off_dt_struct);
    
    u8 *cur = (u8 *)structure_block;
    for(;;)
    {
        u32 tag = devicetree_u32(cur);
        cur += sizeof(u32);
        if(tag == FDT_BEGIN_NODE)
        {
            c8 *name = (c8 *)cur;
            um32 name_len = string_len(name);
            assert(path_len + name_len + 1 < DEVICETREE_MAX_PATH_LEN);
            
            copy_memory(path + path_len, name, name_len);
            path[path_len + name_len] = '/';
            path[path_len + name_len + 1] = '\0';
            path_len += name_len + 1;
            ++dir_depth;
            
            cur += align2_up(name_len + 1, 4);
        }
        else if(tag == FDT_END_NODE)
        {
            assert(dir_depth > 0);
            --dir_depth;
            --path_len; // NOTE: delete the tailing '/'
            while(path_len > 0 && path[path_len - 1] != '/')
                --path_len;
            path[path_len] = '\0';
        }
        else if(tag == FDT_PROP)
        {
            u32 name_offset = devicetree_u32(cur + sizeof(u32));
            c8 *prop_name = strings_block + name_offset;
            u32 prop_size = devicetree_u32(cur);
            u8 *prop = cur + sizeof(u32) * 2;
            
            callback(path, prop_name, prop, prop_size, userdata);
            
            cur += sizeof(u32) * 2 + align2_up(prop_size, 4);
        }
        else if(tag == FDT_NOP)
        {
            do_nothing;
        }
        else if(tag == FDT_END)
        {
            assert(dir_depth == 0 && path_len == 0);
            break;
        }
    }
}