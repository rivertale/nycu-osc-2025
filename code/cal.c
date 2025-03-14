#include <stdio.h>
#include <stdlib.h>

static void
fatal_error(char *message)
{
    printf("%s\n", message);
    exit(1);
}

int
main(int arg_count, char **args)
{
    if(arg_count != 2)
    {
        printf("usage: cal <value>\n");
        return 0;
    }
    
    char *input = args[1];
    
    int mode = 10;
    if(input[0] == '0' && input[1] == 'x')
    {
        mode = 16;
        input += 2;
    }
    
    long long value = 0;
    switch(mode)
    {
        case 10:
        {
            for(char *c = input; *c; ++c)
            {
                if('0' <= *c && *c <= '9')
                    value = value * 10 + (*c - '0'); 
                else
                    fatal_error("invalid digit");
            }
        } break;
        case 16:
        {
            for(char *c = input; *c; ++c)
            {
                int digit = 0;
                if('0' <= *c && *c <= '9')
                    digit = *c - '0';
                else if('A' <= *c && *c <= 'F')
                    digit = 10 + *c - 'A';
                else if('a' <= *c && *c <= 'f')
                    digit = 10 + *c - 'a';
                else
                    fatal_error("invalid digit");
                value = (value << 4) + digit;
            }
        } break;
    }
    
    printf("[value]\n");
    printf("signed = %lld\n", value);
    printf("unsigned = %llu\n", value);
    printf("hex = 0x%llx\n", value);
    printf("[neg]\n");
    printf("signed = %lld\n", -value);
    printf("unsigned = %llu\n", -value);
    printf("hex = 0x%llx\n", -value);
    return 0;
}