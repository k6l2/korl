/** Most of this code is derived from the following sources: 
 * - https://gist.github.com/mmozeiko/b8ccc54037a5eaf35432396feabbe435
 * - https://copyprogramming.com/howto/i-need-some-help-to-understand-usb-game-controllers-hid-devices
 */
#include "korl-windows-gamepad.h"
#include "korl-log.h"
#include "korl-memory.h"
#include "korl-stringPool.h"
#include "korl-stb-ds.h"
#if defined(_LOCAL_STRING_POOL_POINTER)
#   undef _LOCAL_STRING_POOL_POINTER
#endif
#define _LOCAL_STRING_POOL_POINTER (&(_korl_windows_gamepad_context.stringPool))
// https://docs.microsoft.com/windows-hardware/drivers/kernel/defining-i-o-control-codes
// {EC87F1E3-C13B-4100-B5F7-8B84D54260CB}
DEFINE_GUID(_KORL_WINDOWS_GAMEPAD_XBOX_GUID, 0xEC87F1E3, 0xC13B, 0x4100, 0xB5, 0xF7, 0x8B, 0x84, 0xD5, 0x42, 0x60, 0xCB);
// xusb22.sys IOCTLs
#define FILE_DEVICE_XUSB 0x8000U
#define IOCTL_INDEX_XUSB 0x0800U
#define IOCTL_XUSB_GET_INFORMATION              /*0x80006000*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   0, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_XUSB_GET_CAPABILITIES             /*0x8000E004*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   1, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_LED_STATE                /*0x8000E008*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   2, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_STATE                    /*0x8000E00C*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   3, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_SET_STATE                    /*0x8000A010*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   4, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_XUSB_WAIT_GUIDE_BUTTON            /*0x8000E014*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   5, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_BATTERY_INFORMATION      /*0x8000E018*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   6, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_POWER_DOWN                   /*0x8000A01C*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   7, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_AUDIO_DEVICE_INFORMATION /*0x8000E020*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB +   8, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_WAIT_FOR_INPUT               /*0x8000E3AC*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB + 235, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_XUSB_GET_INFORMATION_EX           /*0x8000E3FC*/ CTL_CODE(FILE_DEVICE_XUSB, IOCTL_INDEX_XUSB + 255, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
typedef enum _Korl_Windows_Gamepad_DeviceType
    { _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XBOX
} _Korl_Windows_Gamepad_DeviceType;
typedef struct _Korl_Windows_Gamepad_Device
{
    _Korl_Windows_Gamepad_DeviceType type;
    Korl_StringPool_String path;
    HANDLE handle;
    union
    {
        struct
        {
            DWORD inputFrame;
            WORD buttons;
            union
            {
                struct
                {
                    SHORT leftX;
                    SHORT leftY;
                    SHORT rightX;
                    SHORT rightY;
                } named;
                SHORT array[4];
            } sticks;
            union
            {
                struct
                {
                    BYTE left;
                    BYTE right;
                } named;
                BYTE array[2];
            } triggers;
        } xbox;
    } lastState;
} _Korl_Windows_Gamepad_Device;
typedef struct _Korl_Windows_Gamepad_Context
{
    Korl_Memory_AllocatorHandle allocatorHandle;
    Korl_StringPool stringPool;
    _Korl_Windows_Gamepad_Device* stbDaDevices;
} _Korl_Windows_Gamepad_Context;
korl_global_variable _Korl_Windows_Gamepad_Context _korl_windows_gamepad_context;
korl_internal void _korl_windows_gamepad_connectXbox(LPTSTR devicePath)
{
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    Korl_StringPool_String stringDevicePath = string_newUtf16(devicePath);
    string_toUpper(stringDevicePath);// necessary since apparently SetupDi* API & WM_DEVICECHANGE message structures provide different path cases
    /* check if the device is already registered in our database */
    for(u$ d = 0; d < arrlenu(context->stbDaDevices); d++)
    {
        if(context->stbDaDevices[d].type != _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XBOX)
            continue;
        if(string_equals(&stringDevicePath, &context->stbDaDevices[d].path))
            return;
    }
    /* this device is not registered yet; open a handle to the device */
    HANDLE deviceHandle = CreateFileW(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(deviceHandle == INVALID_HANDLE_VALUE)
    {
        korl_log(WARNING, "failed to open device handle; reconnection required: \"%ws\"", devicePath);
        return;
    }
    /* add the new device to the database */
    const _Korl_Windows_Gamepad_Device newDevice = 
        { .type   = _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XBOX
        , .handle = deviceHandle
        , .path   = stringDevicePath };
    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), 
              context->stbDaDevices, 
              newDevice);
    korl_log(INFO, "xbox gamepad connected: \"%ws\"", string_getRawUtf16(&stringDevicePath));
}
korl_internal void _korl_windows_gamepad_disconnectIndex(u$ devicesIndex)
{
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    korl_assert(devicesIndex < arrlenu(context->stbDaDevices));
    korl_log(INFO, "disconnecting gamepad \"%ws\"", string_getRawUtf16(&context->stbDaDevices[devicesIndex].path));
    string_free(&context->stbDaDevices[devicesIndex].path);
    if(!CloseHandle(context->stbDaDevices[devicesIndex].handle))
        korl_logLastError("CloseHandle failed");
    arrdelswap(context->stbDaDevices, devicesIndex);
}
korl_internal void _korl_windows_gamepad_disconnectPath(LPTSTR devicePath)
{
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    Korl_StringPool_String stringDevicePath = string_newUtf16(devicePath);
    string_toUpper(stringDevicePath);// necessary since apparently SetupDi* API & WM_DEVICECHANGE message structures provide different path cases
    for(u$ d = 0; d < arrlenu(context->stbDaDevices); d++)
        /* if a device in our database matches the devicePath, we need to clean it up and remove it */
        if(string_equals(&stringDevicePath, &context->stbDaDevices[d].path))
        {
            _korl_windows_gamepad_disconnectIndex(d);
            goto cleanUp;
        }
    korl_log(INFO, "device disconnected, but not present in database: \"%ws\"", string_getRawUtf16(&stringDevicePath));
    cleanUp:
    string_free(&stringDevicePath);
}
korl_internal void korl_windows_gamepad_initialize(void)
{
    korl_memory_zero(&_korl_windows_gamepad_context, sizeof(_korl_windows_gamepad_context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_kilobytes(128);
    _korl_windows_gamepad_context.allocatorHandle = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_CRT, L"korl-gamepad", KORL_MEMORY_ALLOCATOR_FLAGS_NONE, &heapCreateInfo);
    _korl_windows_gamepad_context.stringPool      = korl_stringPool_create(_korl_windows_gamepad_context.allocatorHandle);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_windows_gamepad_context.allocatorHandle), _korl_windows_gamepad_context.stbDaDevices, 8);
}
korl_internal void korl_windows_gamepad_registerWindow(HWND windowHandle, Korl_Memory_AllocatorHandle allocatorHandleLocal)
{
    /* register XBOX gamepad devices to send messages to the provided window */
    KORL_ZERO_STACK(DEV_BROADCAST_DEVICEINTERFACE_W, deviceNotificationFilter);
    deviceNotificationFilter.dbcc_size       = sizeof(deviceNotificationFilter);
    deviceNotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    deviceNotificationFilter.dbcc_classguid  = _KORL_WINDOWS_GAMEPAD_XBOX_GUID;
    if(!RegisterDeviceNotification(windowHandle, &deviceNotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE))
        korl_logLastError("RegisterDeviceNotification failed");
    /* enumerate & identify relevant devices before we process any device window messages */
    HDEVINFO deviceInfos = SetupDiGetClassDevs(&_KORL_WINDOWS_GAMEPAD_XBOX_GUID, NULL/*enumerator*/, NULL/*hwndParent*/, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if(deviceInfos == INVALID_HANDLE_VALUE)
        korl_logLastError("SetupDiGetClassDevs failed");
    else
    {
        KORL_ZERO_STACK(SP_DEVICE_INTERFACE_DATA, deviceInterfaceData);
        deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);
        for(DWORD deviceIndex = 0; 
            SetupDiEnumDeviceInterfaces(deviceInfos, NULL/*deviceInfoData*/, &_KORL_WINDOWS_GAMEPAD_XBOX_GUID, deviceIndex, &deviceInterfaceData);
            korl_memory_zero(&deviceInterfaceData, sizeof(deviceInterfaceData))
            , deviceInterfaceData.cbSize = sizeof(deviceInterfaceData)
            , deviceIndex++)
        {
            DWORD deviceInterfaceDetailBytes;
            // query for the size of the device interface detail data:
            korl_assert(!SetupDiGetDeviceInterfaceDetail(deviceInfos, &deviceInterfaceData, 
                                                         NULL/*deviceInterfaceDetailData; NULL=>query for size*/, 
                                                         0/*detailDataBytes; 0=>query for this*/, 
                                                         &deviceInterfaceDetailBytes, NULL/*deviceInfoData*/));// yes, this should always return false when querying for the detail bytes
            if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)// when called as above, SetupDiGetDeviceInterfaceDetail should always set the last error code to ERROR_INSUFFICIENT_BUFFER
                korl_logLastError("SetupDiGetDeviceInterfaceDetail failed");
            PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetailData = korl_allocate(allocatorHandleLocal, deviceInterfaceDetailBytes);
            korl_assert(pDeviceInterfaceDetailData);
            pDeviceInterfaceDetailData->cbSize = sizeof(*pDeviceInterfaceDetailData);// NOTE: _not_ the size of the allocation
            KORL_ZERO_STACK(SP_DEVINFO_DATA, deviceInfoData);
            deviceInfoData.cbSize = sizeof(deviceInfoData);
            // now we can actually get all the device interface detail data:
            if(!SetupDiGetDeviceInterfaceDetail(deviceInfos, &deviceInterfaceData, pDeviceInterfaceDetailData, deviceInterfaceDetailBytes, &deviceInterfaceDetailBytes, &deviceInfoData))
                korl_logLastError("SetupDiGetDeviceInterfaceDetail failed");
            _korl_windows_gamepad_connectXbox(pDeviceInterfaceDetailData->DevicePath);
            korl_free(allocatorHandleLocal, pDeviceInterfaceDetailData);
        }
    }
}
korl_internal bool korl_windows_gamepad_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* out_result)
{
    korl_assert(out_result);
    switch(message)
    {
    case WM_DEVICECHANGE:{
        switch(wParam)
        {
        case DBT_DEVICEARRIVAL:{
            DEV_BROADCAST_HDR*const broadcastHeader = KORL_C_CAST(void*, lParam);
            if(broadcastHeader->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                DEV_BROADCAST_DEVICEINTERFACE_W*const deviceInterface = KORL_C_CAST(void*, broadcastHeader);
                if(IsEqualGUID(&deviceInterface->dbcc_classguid, &_KORL_WINDOWS_GAMEPAD_XBOX_GUID))
                    _korl_windows_gamepad_connectXbox(deviceInterface->dbcc_name);
                else
                    out_result = NULL;
            }
            else
                out_result = NULL;
            break;}
        case DBT_DEVICEREMOVECOMPLETE:{
            DEV_BROADCAST_HDR*const broadcastHeader = KORL_C_CAST(void*, lParam);
            if(broadcastHeader->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                DEV_BROADCAST_DEVICEINTERFACE_W*const deviceInterface = KORL_C_CAST(void*, broadcastHeader);
                _korl_windows_gamepad_disconnectPath(deviceInterface->dbcc_name);
            }
            else
                out_result = NULL;
            break;}
        }
        if(out_result)
            *out_result = 0;}
    default:{
        out_result = NULL;}
    }
    return out_result != NULL;
}
korl_internal void _korl_windows_gamepad_xInputFilterStickDeadzone(SHORT* inOut_stickX, SHORT* inOut_stickY, SHORT deadzoneMagnitude)
{
    f32 fX = *inOut_stickX;
    f32 fY = *inOut_stickY;
    f32 magnitude = korl_math_f32_squareRoot(fX*fX + fY*fY);
    if(magnitude <= deadzoneMagnitude)
    {
        *inOut_stickX = 0;
        *inOut_stickY = 0;
        return;
    }
#if KORL_DEBUG
    korl_assert(deadzoneMagnitude > 0);
#endif
    fX /= magnitude;
    fY /= magnitude;
    if(magnitude > KORL_I16_MAX)
        magnitude = KORL_I16_MAX;
    magnitude = ((magnitude - deadzoneMagnitude) / (KORL_I16_MAX - deadzoneMagnitude)) * KORL_I16_MAX;
    *inOut_stickX = KORL_C_CAST(SHORT, fX*magnitude);
    *inOut_stickY = KORL_C_CAST(SHORT, fY*magnitude);
}
korl_internal void _korl_windows_gamepad_xInputFilterTriggerDeadzone(BYTE* inOut_trigger, BYTE deadzoneMagnitude)
{
    if(*inOut_trigger <= deadzoneMagnitude)
    {
        *inOut_trigger = 0;
        return;
    }
#if KORL_DEBUG
    korl_assert(deadzoneMagnitude > 0);
#endif
    const f32 fX = KORL_C_CAST(f32, (*inOut_trigger - deadzoneMagnitude)) / (KORL_U8_MAX - deadzoneMagnitude);
    *inOut_trigger = KORL_C_CAST(BYTE, fX*KORL_U8_MAX);
}
korl_internal void korl_windows_gamepad_poll(fnSig_korl_game_onGamepadEvent* onGamepadEvent)
{
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    for(u$ i = arrlenu(context->stbDaDevices) - 1; i < arrlenu(context->stbDaDevices); i--)
    {
        if(context->stbDaDevices[i].type != _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XBOX)
            continue;// only support XBOX gamepads (for now)
        BYTE in[3] = { 0x01, 0x01, 0x00 };//KORL-ISSUE-000-000-083: gamepad: XBOX wireless devices only support a single gamepad instead of 4
        BYTE out[29];
        DWORD size;
        if(   !DeviceIoControl(context->stbDaDevices[i].handle, IOCTL_XUSB_GET_STATE, in, sizeof(in), out, sizeof(out), &size, NULL) 
           || size != sizeof(out))
        {
            switch(GetLastError())
            {
            case ERROR_DEVICE_NOT_CONNECTED:{
                /* We can't just disconnect the XBOX controller device here, since 
                    it is entirely possible that this is a wireless controller receiver, 
                    which can communicate with multiple controllers. */
                // korl_log(INFO, "DeviceIoControl => ERROR_DEVICE_NOT_CONNECTED; disconnecting gamepad...");
                continue;}
            default: {
                korl_logLastError("DeviceIoControl failed");
                continue;}
            }
        }
        const _Korl_Windows_Gamepad_Device deviceStatePrevious = context->stbDaDevices[i];
        korl_assert(korl_memory_isLittleEndian());// the raw xbox state data is stored in the buffer in little-endian byte order; so we can only C-cast into multi-byte registers if our platform is also little-endian
        context->stbDaDevices[i].lastState.xbox.inputFrame           = *KORL_C_CAST(DWORD*, out +  5);
        context->stbDaDevices[i].lastState.xbox.buttons              = *KORL_C_CAST(WORD* , out + 11);
        context->stbDaDevices[i].lastState.xbox.sticks.named.leftX   = *KORL_C_CAST(SHORT*, out + 15);
        context->stbDaDevices[i].lastState.xbox.sticks.named.leftY   = *KORL_C_CAST(SHORT*, out + 17);
        context->stbDaDevices[i].lastState.xbox.sticks.named.rightX  = *KORL_C_CAST(SHORT*, out + 19);
        context->stbDaDevices[i].lastState.xbox.sticks.named.rightY  = *KORL_C_CAST(SHORT*, out + 21);
        context->stbDaDevices[i].lastState.xbox.triggers.named.left  = out[13];
        context->stbDaDevices[i].lastState.xbox.triggers.named.right = out[14];
        /** deadzone filter */
        //KORL-FEATURE-000-000-036: gamepad: (low priority); expose gamepad deadzone values as configurable through the KORL platform interface
        /* these deadzone values are derived from: 
            https://docs.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput#dead-zone */
        #define _KORL_XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
        #define _KORL_XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
        #define _KORL_XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30
        _korl_windows_gamepad_xInputFilterStickDeadzone(&context->stbDaDevices[i].lastState.xbox.sticks.named.leftX, 
                                                        &context->stbDaDevices[i].lastState.xbox.sticks.named.leftY, 
                                                        _KORL_XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        _korl_windows_gamepad_xInputFilterStickDeadzone(&context->stbDaDevices[i].lastState.xbox.sticks.named.rightX, 
                                                        &context->stbDaDevices[i].lastState.xbox.sticks.named.rightY, 
                                                        _KORL_XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        _korl_windows_gamepad_xInputFilterTriggerDeadzone(&context->stbDaDevices[i].lastState.xbox.triggers.named.left, 
                                                          _KORL_XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        _korl_windows_gamepad_xInputFilterTriggerDeadzone(&context->stbDaDevices[i].lastState.xbox.triggers.named.right, 
                                                          _KORL_XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        /** these flags are presented in the exact order that the corresponding 
         * KORL button value in the Korl_GamepadButton enumeration; it is 
         * essentially a mapping from the xbox_button_bitfield => KORL_button_index */
        korl_shared_const WORD XBOX_BUTTON_FLAGS[] = 
            { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080
            , 0x0100, 0x0200, 0x0400,         0x1000, 0x2000, 0x4000, 0x8000 };
        //KORL-ISSUE-000-000-084: gamepad: (low priority) XBOX controller guide button is not being captured
        for(u$ b = 0; b < korl_arraySize(XBOX_BUTTON_FLAGS); b++)
        {
            if(   (context->stbDaDevices[i].lastState.xbox.buttons & XBOX_BUTTON_FLAGS[b])
               == (     deviceStatePrevious.lastState.xbox.buttons & XBOX_BUTTON_FLAGS[b]))
                continue;
            if(!onGamepadEvent)
                continue;
            onGamepadEvent((Korl_GamepadEvent){.type = KORL_GAMEPAD_EVENT_TYPE_BUTTON
                                              ,.subType = {.button = {.button  = KORL_C_CAST(Korl_GamepadButton, b)
                                                                     ,.pressed = context->stbDaDevices[i].lastState.xbox.buttons & XBOX_BUTTON_FLAGS[b]}}});
        }
        for(u$ a = 0; a < korl_arraySize(deviceStatePrevious.lastState.xbox.sticks.array); a++)
        {
            if(context->stbDaDevices[i].lastState.xbox.sticks.array[a] != deviceStatePrevious.lastState.xbox.sticks.array[a])
                if(onGamepadEvent)
                    onGamepadEvent((Korl_GamepadEvent){.type = KORL_GAMEPAD_EVENT_TYPE_AXIS
                                                      ,.subType = {.axis = {.axis  = KORL_C_CAST(Korl_GamepadAxis, a)
                                                                           ,.value = KORL_C_CAST(f32, context->stbDaDevices[i].lastState.xbox.sticks.array[a]) / KORL_I16_MAX}}});
        }
        for(u$ t = 0; t < korl_arraySize(deviceStatePrevious.lastState.xbox.triggers.array); t++)
        {
            if(context->stbDaDevices[i].lastState.xbox.triggers.array[t] != deviceStatePrevious.lastState.xbox.triggers.array[t])
                if(onGamepadEvent)
                    onGamepadEvent((Korl_GamepadEvent){.type = KORL_GAMEPAD_EVENT_TYPE_AXIS
                                                      ,.subType = {.axis = {.axis  = KORL_C_CAST(Korl_GamepadAxis, t + KORL_GAMEPAD_AXIS_TRIGGER_LEFT)
                                                                           ,.value = KORL_C_CAST(f32, context->stbDaDevices[i].lastState.xbox.triggers.array[t]) / KORL_U8_MAX}}});
        }
    }
}
#undef _LOCAL_STRING_POOL_POINTER
