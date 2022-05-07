#pragma once
#include "korl-globalDefines.h"
typedef enum Korl_CommandLine_ArgumentType
{
    KORL_COMMAND_LINE_ARGUMENT_TYPE_BOOL,
} Korl_CommandLine_ArgumentType;
typedef struct Korl_CommandLine_ArgumentDescriptor
{
    void* data;
    Korl_CommandLine_ArgumentType type;
    const wchar_t* argument;
    const wchar_t* argumentAlias;
    const wchar_t* description;
} Korl_CommandLine_ArgumentDescriptor;
/** \return \c true if the program should immediately end execution, such as is 
 * the case when the program is called with the \c --help argument.  Returns 
 * \c false otherwise. */
korl_internal void korl_commandLine_parse   (const Korl_CommandLine_ArgumentDescriptor* descriptors, u$ descriptorCount);
korl_internal void korl_commandLine_logUsage(const Korl_CommandLine_ArgumentDescriptor* descriptors, u$ descriptorCount);
