#include "korl-commandLine.h"
#include "korl-log.h"
#include "korl-assert.h"
#include "korl-memory.h"
#include "korl-checkCast.h"
korl_internal void korl_commandLine_parse(const Korl_CommandLine_ArgumentDescriptor* descriptors, u$ descriptorCount)
{
    /* initialize argument data */
    for(u$ i = 0; i < descriptorCount; i++)
        switch(descriptors[i].type)
        {
        case KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL:{
            *KORL_C_CAST(bool*, descriptors[i].data) = false;
            break;}
        default:{
            korl_assert(!"argument type not implemented");
            break;}
        }
    wchar_t* cStringCommandLine = GetCommandLine();// memory managed by Windows
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(cStringCommandLine, &argc);
    korl_assert(argv);
    for(int a = 0; a < argc; a++)
    {
        for(u$ i = 0; i < descriptorCount; i++)
            if(    0 == korl_memory_stringCompare(argv[a], descriptors[i].argumentAlias)
                || 0 == korl_memory_stringCompare(argv[a], descriptors[i].argument))
                switch(descriptors[i].type)
                {
                case KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL:{
                    *KORL_C_CAST(bool*, descriptors[i].data) = true;
                    break;}
                default:{
                    korl_assert(!"argument type not implemented");
                    break;}
                }
    }
    korl_assert(LocalFree(argv) == NULL);
}
korl_internal void korl_commandLine_logUsage(const Korl_CommandLine_ArgumentDescriptor* descriptors, u$ descriptorCount)
{
    int argumentColumnWidthMax = 0;
    for(u$ i = 0; i < descriptorCount; i++)
    {
        const int argumentColumnWidth = korl_checkCast_u$_to_i32(
              korl_memory_stringSize(descriptors[i].argument) 
            + korl_memory_stringSize(L", ")
            + korl_memory_stringSize(descriptors[i].argumentAlias));
        argumentColumnWidthMax = KORL_MATH_MAX(argumentColumnWidthMax, argumentColumnWidth);
    }
    korl_shared_const wchar_t ARGUMENT_HEADER[] = L"┏┫Argument┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
    korl_log(INFO, "%.*ws━━┳┫Description┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓", 
             argumentColumnWidthMax + 1/*'┃' on the left side*/, ARGUMENT_HEADER);
    for(u$ i = 0; i < descriptorCount; i++)
    {
        const int argumentColumnWidth = korl_checkCast_u$_to_i32(
              korl_memory_stringSize(descriptors[i].argument) 
            + korl_memory_stringSize(L", ")
            + korl_memory_stringSize(descriptors[i].argumentAlias));
        korl_log(INFO, "┃%*ws ┃\n"
                       "┃%ws, %ws%*ws ┃ %ws", 
                 argumentColumnWidthMax + 1/*'┃' on the left side*/, L"",
                 descriptors[i].argument, descriptors[i].argumentAlias, 
                 argumentColumnWidthMax + 1/*'┃' on the left side*/ - argumentColumnWidth, L"",
                 descriptors[i].description);
    }
}
