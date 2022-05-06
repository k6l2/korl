#include "korl-commandLine.h"
#include "korl-log.h"
#include "korl-assert.h"
korl_internal bool korl_commandLine_parse(const Korl_CommandLine_ArgumentDescriptor* descriptors, u$ descriptorCount)
{
    bool resultEndProgram = false;
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
        if(0 == korl_memory_stringCompare(argv[a], L"--help"))
        {
            resultEndProgram = true;
            break;
        }
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
    return resultEndProgram;
}
korl_internal void korl_commandLine_logUsage(const Korl_CommandLine_ArgumentDescriptor* descriptors, u$ descriptorCount)
{
    for(u$ i = 0; i < descriptorCount; i++)
        korl_log(INFO, "%ws, %ws\n\t%ws", 
                 descriptors[i].argument, descriptors[i].argumentAlias, 
                 descriptors[i].description);
}
