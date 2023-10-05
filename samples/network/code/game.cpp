#include "korl-interface-game.h"// includes "korl-interface-platform.h"
#include "utility/korl-stringPool.h"
#include "utility/korl-logConsole.h"
#include "utility/korl-camera-freeFly.h"
#include "utility/korl-utility-gfx.h"
#include "utility/korl-utility-string.h"
#include "utility/korl-utility-stb-ds.h"
#define RECEIVER_LISTEN_PORT 12345
typedef struct Memory
{
    Korl_Memory_AllocatorHandle allocatorHeap;
    Korl_Memory_AllocatorHandle allocatorFrame;
    bool                        quit;
    Korl_StringPool             stringPool;// used by logConsole
    Korl_LogConsole             logConsole;
    Korl_Network_Socket         socketReceiver;
    Korl_Network_Socket         socketSender;
    Korl_Network_Address        netAddressSend;
} Memory;
korl_global_variable Memory* memory;
KORL_EXPORT KORL_FUNCTION_korl_command_callback(command_resolve)
{
    memory->netAddressSend = korl_network_resolveAddress(parameters[1]);
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(command_socketOpen)
{
    korl_log(INFO, "opening UDP sockets...");
    memory->socketSender   = korl_network_openUdp(0);
    korl_log(INFO, "sender open; listening on all ports, but we can just ignore that");
    memory->socketReceiver = korl_network_openUdp(RECEIVER_LISTEN_PORT);
    korl_log(INFO, "receiver open; listening on port %i", RECEIVER_LISTEN_PORT);
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(command_socketClose)
{
    korl_log(INFO, "closing UDP sockets...");
    korl_network_close(memory->socketSender);
    memory->socketSender = 0;
    korl_log(INFO, "sender closed.");
    korl_network_close(memory->socketReceiver);
    memory->socketReceiver = 0;
    korl_log(INFO, "receiver closed.");
}
KORL_EXPORT KORL_FUNCTION_korl_command_callback(command_send)
{
    korl_log(INFO, "sending data \"%.*hs\"", parameters[1].size, parameters[1].data);
    const i32 bytesSent = korl_network_send(memory->socketSender, parameters[1].data, parameters[1].size, &memory->netAddressSend, RECEIVER_LISTEN_PORT);
    korl_log(INFO, "sent %i bytes over UDP", bytesSent);
}
KORL_EXPORT KORL_GAME_INITIALIZE(korl_game_initialize)
{
    korl_platform_getApi(korlApi);
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_megabytes(8);
    const Korl_Memory_AllocatorHandle allocatorHeap = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"game", KORL_MEMORY_ALLOCATOR_FLAG_SERIALIZE_SAVE_STATE, &heapCreateInfo);
    memory = KORL_C_CAST(Memory*, korl_allocate(allocatorHeap, sizeof(Memory)));
    memory->allocatorHeap  = allocatorHeap;
    memory->allocatorFrame = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"game-stack", KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME, &heapCreateInfo);
    memory->stringPool     = korl_stringPool_create(allocatorHeap);
    memory->logConsole     = korl_logConsole_create(&memory->stringPool);
    memory->netAddressSend = korl_network_resolveAddress(KORL_RAW_CONST_UTF8("localhost"));
    korl_gui_setFontAsset(L"data/source-sans/SourceSans3-Semibold.otf");// KORL-ISSUE-000-000-086: gfx: default font path doesn't work, since this subdirectly is unlikely in the game project
    korl_command_register(KORL_RAW_CONST_UTF8("net-resolve") , command_resolve);
    korl_command_register(KORL_RAW_CONST_UTF8("socket-open") , command_socketOpen);
    korl_command_register(KORL_RAW_CONST_UTF8("socket-close"), command_socketClose);
    korl_command_register(KORL_RAW_CONST_UTF8("net-send")    , command_send);
    return memory;
}
KORL_EXPORT KORL_GAME_ON_RELOAD(korl_game_onReload)
{
    korl_platform_getApi(korlApi);
    memory = KORL_C_CAST(Memory*, context);
}
KORL_EXPORT KORL_GAME_ON_KEYBOARD_EVENT(korl_game_onKeyboardEvent)
{
    if(isDown && !isRepeat)
        switch(keyCode)
        {
        case KORL_KEY_ESCAPE:{memory->quit = true; break;}
        case KORL_KEY_GRAVE :{korl_logConsole_toggle(&memory->logConsole); break;}
        default: break;
        }
}
KORL_EXPORT KORL_GAME_UPDATE(korl_game_update)
{
    /* test "server" logic */
    if(memory->socketReceiver)
    {
        u8 udpBuffer[KORL_NETWORK_MAX_DATAGRAM_BYTES];// for now, it is generally necessary to use the largest possible buffer, as korl-network considers a receive buffer overflow as an error; I may change this in the future if it's possible to "peek" the next datagram to check its size
        struct
        {
            Korl_Network_Address address;
            u16                  port;
        } sender;
        const i32 bytesReceived = korl_network_receive(memory->socketReceiver, udpBuffer, sizeof(udpBuffer), &sender.address, &sender.port);
        if(bytesReceived)
            korl_log(INFO, "SERVER: received %i bytes: %.*hs", bytesReceived, bytesReceived, udpBuffer);
    }
    korl_logConsole_update(&memory->logConsole, deltaSeconds, korl_log_getBuffer, {windowSizeX, windowSizeY}, memory->allocatorFrame);
    return !memory->quit;
}
#include "utility/korl-utility-stb-ds.c"
#include "utility/korl-stringPool.c"
#include "utility/korl-checkCast.c"
#include "utility/korl-utility-math.c"
#include "utility/korl-logConsole.c"
#include "utility/korl-camera-freeFly.c"
#include "utility/korl-utility-gfx.c"
#include "utility/korl-utility-string.c"
#include "utility/korl-utility-memory.c"
