/** Most of this code is derived from the following sources: 
 * - https://gist.github.com/mmozeiko/b8ccc54037a5eaf35432396feabbe435
 * - https://copyprogramming.com/howto/i-need-some-help-to-understand-usb-game-controllers-hid-devices
 * - https://github.com/MysteriousJ/Joystick-Input-Examples
 */
#include "korl-windows-gamepad.h"
#include "korl-log.h"
#include "korl-memory.h"
#include "utility/korl-stringPool.h"
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
    { _KORL_WINDOWS_GAMEPAD_DEVICETYPE_INVALID
    , _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XINPUT
    , _KORL_WINDOWS_GAMEPAD_DEVICETYPE_DUALSHOCK4
} _Korl_Windows_Gamepad_DeviceType;
typedef enum _Korl_Windows_Gamepad_ConnectionType
    { _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_UNKNOWN// for some devices, such as DS4, we cannot determine what type of connection the device has from RawInput/HID data alone; for example, we cannot determine the connection type of DS4 until we see the byte size of the first input report
    , _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_USB
    , _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_BLUETOOTH// bluetooth devices on Windows, once paired, appear to _always_ be "connected", at least if we're only looking at RawInput's WM_INPUT_DEVICE_CHANGE events; in this case, we will need to use some other approach to determine whether or not a device is connected, such as the duration since the last RawInput report for example
} _Korl_Windows_Gamepad_ConnectionType;
/**
 * \note: when I mention "(dis)connection" for Gamepad_Device here, that does 
 *     _not_ necessarily mean physical device connection to the machine
 * @TODO: maybe I should just refactor all the mentions of "(dis)connection" in 
 *     this module with a more fitting term, such as "(un)registration"; actual 
 *     device connection technique will likely vary depending on the device; for 
 *     example, XINPUT devices will need to be polled to determine what 
 *     controllers are connected to it, and BLUETOOTH devices such as DS4 will 
 *     likely need some kind of metric to determine if the device is still 
 *     connected: for example, we could periodically attempt communication with 
 *     the device by sending an output report and base the connection off the 
 *     result of that command; TLDR: connection & registration seem to be two 
 *     distinct states that need to be tracked _seperately_!
 */
typedef struct _Korl_Windows_Gamepad_Device
{
    _Korl_Windows_Gamepad_DeviceType     type;// should _never_ == _KORL_WINDOWS_GAMEPAD_DEVICETYPE_INVALID
    _Korl_Windows_Gamepad_ConnectionType connectionType;
    Korl_StringPool_String               path;// used to create handleFileIo, & uniquely identify the local device
    HANDLE                               handleRawInputDevice;// managed by Windows; set to INVALID_HANDLE_VALUE upon gamepad device disconnection
    HANDLE                               handleFileIo;// managed by korl-windows-gamepad; set to INVALID_HANDLE_VALUE upon gamepad device disconnection
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
    //@TODO: unify "lastState" struct to just store an array of KORL_BUTTON & KORL_AXIS value, defined in `korl-interface-platform-input.h`
} _Korl_Windows_Gamepad_Device;
typedef struct _Korl_Windows_Gamepad_Context
{
    Korl_Memory_AllocatorHandle   allocatorHandle;
    Korl_Memory_AllocatorHandle   allocatorHandleStack;
    Korl_StringPool               stringPool;
    _Korl_Windows_Gamepad_Device* stbDaDevices;// devices are uniquely identified by a String, and thus we will _never_ remove them from the database for the entire duration of program execution; this will allow KORL clients to easily perform logic based on which gamepad using its unique id; the device's unique id will simply be its index into this array
} _Korl_Windows_Gamepad_Context;
korl_global_variable _Korl_Windows_Gamepad_Context _korl_windows_gamepad_context;
korl_internal void _korl_windows_gamepad_device_disconnect(_Korl_Windows_Gamepad_Device* device)
{
    if(   device->handleFileIo         == INVALID_HANDLE_VALUE 
       && device->handleRawInputDevice == INVALID_HANDLE_VALUE)
        return;// device was already disconnected
    KORL_WINDOWS_CHECK(CloseHandle(device->handleFileIo));
    device->handleFileIo         = INVALID_HANDLE_VALUE;
    device->handleRawInputDevice = INVALID_HANDLE_VALUE;
    korl_log(INFO, "device disconnected: \"%ws\"", string_getRawUtf16(&device->path));
}
korl_internal void _korl_windows_gamepad_connectXbox(LPTSTR devicePath)
{
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    Korl_StringPool_String stringDevicePath = string_newUtf16(devicePath);
    string_toUpper(stringDevicePath);// necessary since apparently SetupDi* API & WM_DEVICECHANGE message structures provide different path cases
    /* check if the device is already registered in our database */
    _Korl_Windows_Gamepad_Device* newDevice = NULL;
    const _Korl_Windows_Gamepad_Device*const devicesEnd = context->stbDaDevices + arrlen(context->stbDaDevices);
    for(_Korl_Windows_Gamepad_Device* device = context->stbDaDevices
       ;device < devicesEnd && !newDevice
       ;device++)
        if(   device->type == _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XINPUT
           && string_equals(&stringDevicePath, &device->path))
        {
            newDevice = device;
            string_free(&stringDevicePath);
        }
    /* if this device is not registered, add a new entry */
    if(!newDevice)
    {
        mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaDevices, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Windows_Gamepad_Device));
        newDevice = &arrlast(context->stbDaDevices);
        newDevice->type                 = _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XINPUT;
        newDevice->path                 = stringDevicePath;
        newDevice->handleRawInputDevice = INVALID_HANDLE_VALUE;
        newDevice->handleFileIo         = INVALID_HANDLE_VALUE;
    }
    /* open a new file handle to the device */
    newDevice->handleFileIo = CreateFileW(devicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(newDevice->handleFileIo == INVALID_HANDLE_VALUE)
        korl_log(WARNING, "failed to open device handle; reconnection required: \"%ws\"", devicePath);
    /**/
    korl_log(INFO, "xbox gamepad [%lli] connected: \"%ws\"", newDevice - context->stbDaDevices, string_getRawUtf16(&newDevice->path));
}
#if 0
korl_internal void _korl_windows_gamepad_disconnectIndex(u$ devicesIndex)
{
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    korl_assert(devicesIndex < arrlenu(context->stbDaDevices));
    korl_log(INFO, "disconnecting gamepad \"%ws\"", string_getRawUtf16(&context->stbDaDevices[devicesIndex].path));
    string_free(&context->stbDaDevices[devicesIndex].path);
    if(!CloseHandle(context->stbDaDevices[devicesIndex].handleFileIo))
        korl_logLastError("CloseHandle failed");
    arrdelswap(context->stbDaDevices, devicesIndex);
}
korl_internal void _korl_windows_gamepad_disconnectPath(LPTSTR devicePath)
{
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    Korl_StringPool_String stringDevicePath = string_newUtf16(devicePath);
    string_toUpper(stringDevicePath);// necessary since apparently SetupDi* API & WM_DEVICECHANGE message structures provide different path cases
    const _Korl_Windows_Gamepad_Device*const devicesEnd = context->stbDaDevices + arrlen(context->stbDaDevices);
    for(_Korl_Windows_Gamepad_Device* device = context->stbDaDevices
       ;device < devicesEnd
       ;device++)
        /* if a device in our database matches the devicePath, we need to clean it up and remove it */
        if(string_equals(&stringDevicePath, &device->path))
        {
            KORL_WINDOWS_CHECK(CloseHandle(device->handleFileIo));
            device->handleFileIo = INVALID_HANDLE_VALUE;
            korl_log(INFO, "device disconnected: \"%ws\"", string_getRawUtf16(&stringDevicePath));
            goto cleanUp;
        }
    korl_log(WARNING, "device disconnected, but not present in database: \"%ws\"", string_getRawUtf16(&stringDevicePath));
    cleanUp:
        string_free(&stringDevicePath);
}
#endif
korl_internal void korl_windows_gamepad_initialize(void)
{
    korl_memory_zero(&_korl_windows_gamepad_context, sizeof(_korl_windows_gamepad_context));
    KORL_ZERO_STACK(Korl_Heap_CreateInfo, heapCreateInfo);
    heapCreateInfo.initialHeapBytes = korl_math_kilobytes(128);
    _korl_windows_gamepad_context.allocatorHandle      = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_CRT   , L"korl-gamepad"      , KORL_MEMORY_ALLOCATOR_FLAGS_NONE            , &heapCreateInfo);
    _korl_windows_gamepad_context.allocatorHandleStack = korl_memory_allocator_create(KORL_MEMORY_ALLOCATOR_TYPE_LINEAR, L"korl-gamepad-stack", KORL_MEMORY_ALLOCATOR_FLAG_EMPTY_EVERY_FRAME, &heapCreateInfo);
    _korl_windows_gamepad_context.stringPool           = korl_stringPool_create(_korl_windows_gamepad_context.allocatorHandle);
    mcarrsetcap(KORL_STB_DS_MC_CAST(_korl_windows_gamepad_context.allocatorHandle), _korl_windows_gamepad_context.stbDaDevices, 8);
}
korl_internal void korl_windows_gamepad_registerWindow(HWND windowHandle, Korl_Memory_AllocatorHandle allocatorHandleLocal)
{
    /* register RawInput devices */
    RAWINPUTDEVICE deviceList[2];
    deviceList[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    deviceList[0].usUsage     = HID_USAGE_GENERIC_GAMEPAD;
    deviceList[0].dwFlags     = RIDEV_INPUTSINK/*receive messages even if window is not in foreground*/
                              | RIDEV_DEVNOTIFY/*receive arrival/removal messages via WM_INPUT_DEVICE_CHANGE*/;
    deviceList[0].hwndTarget  = windowHandle;
    deviceList[1] = deviceList[0];
    deviceList[1].usUsage = HID_USAGE_GENERIC_JOYSTICK;
    KORL_WINDOWS_CHECK(RegisterRawInputDevices(deviceList, korl_arraySize(deviceList), sizeof(*deviceList)));
    /* enumerate over RawInput devices, in case we started the application with them already connected */
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
        for(DWORD deviceIndex = 0
           ;SetupDiEnumDeviceInterfaces(deviceInfos, NULL/*deviceInfoData*/, &_KORL_WINDOWS_GAMEPAD_XBOX_GUID, deviceIndex, &deviceInterfaceData)
           ;korl_memory_zero(&deviceInterfaceData, sizeof(deviceInterfaceData))
            , deviceInterfaceData.cbSize = sizeof(deviceInterfaceData)
            , deviceIndex++)
        {
            DWORD deviceInterfaceDetailBytes;
            // query for the size of the device interface detail data:
            korl_assert(!SetupDiGetDeviceInterfaceDetail(deviceInfos, &deviceInterfaceData
                                                        ,NULL/*deviceInterfaceDetailData; NULL=>query for size*/
                                                        ,0/*detailDataBytes; 0=>query for this*/
                                                        ,&deviceInterfaceDetailBytes, NULL/*deviceInfoData*/));// yes, this should always return false when querying for the detail bytes
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
        KORL_WINDOWS_CHECK(SetupDiDestroyDeviceInfoList(deviceInfos));
    }
}
korl_internal bool korl_windows_gamepad_processMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* out_result, fnSig_korl_game_onGamepadEvent* onGamepadEvent)
{
    korl_shared_const u32 RAW_INPUT_BUFFER_BYTE_ALIGNMENT = 8;// I can't find this on MSDN, but empirically (from what I can tell so far) _all_ data buffers RawInput writes to _must_ be aligned to 8 bytes on x64 builds!  Failing to allocate write buffers using proper alignment will cause RawInput calls to fail & set GetLastError() to ERROR_NOACCESS(0x000003E6); See: https://stackoverflow.com/a/24499844/4526664
    _Korl_Windows_Gamepad_Context*const context = &_korl_windows_gamepad_context;
    korl_assert(out_result);
    switch(message)
    {
    case WM_INPUT:{
        const HRAWINPUT handleRawInput = KORL_C_CAST(HRAWINPUT, lParam);
        switch(GET_RAWINPUT_CODE_WPARAM(wParam))
        {
        case RIM_INPUT:{// window is in foreground; DefWindowProc _must_ be called for system clean up
            /* obtain the HANDLE of the device generating this RawInput data */
            UINT rawInputBytes = 0;
            UINT resultRawInput = GetRawInputData(handleRawInput, RID_INPUT, NULL, &rawInputBytes, sizeof(RAWINPUTHEADER));
            if(resultRawInput != 0)
            {
                korl_log(WARNING, "GetRawInputData (size check) failed; GetLastError=0x%x", GetLastError());
                break;
            }
            RAWINPUT* rawInput = korl_allocateAlignedDirty(context->allocatorHandleStack, rawInputBytes, RAW_INPUT_BUFFER_BYTE_ALIGNMENT);
            resultRawInput = GetRawInputData(handleRawInput, RID_INPUT, rawInput, &rawInputBytes, sizeof(RAWINPUTHEADER));
            if(resultRawInput == KORL_C_CAST(UINT, -1))
            {
                korl_log(WARNING, "GetRawInputData failed; GetLastError=0x%x", GetLastError());
                break;
            }
            /* find the device with a matching RawInput handle in our database (if it exists) */
            _Korl_Windows_Gamepad_Device* device = NULL;
            const _Korl_Windows_Gamepad_Device*const gamepadDevicesEnd = context->stbDaDevices + arrlen(context->stbDaDevices);
            for(_Korl_Windows_Gamepad_Device* currentDevice = context->stbDaDevices
               ;currentDevice < gamepadDevicesEnd && !device
               ;currentDevice++)
                if(currentDevice->handleRawInputDevice == rawInput->header.hDevice)
                    device = currentDevice;
            if(!device)
                break;
            /* get RawInput input reports from the device */
            switch(device->type)
            {
            case _KORL_WINDOWS_GAMEPAD_DEVICETYPE_DUALSHOCK4:{
                const _Korl_Windows_Gamepad_ConnectionType connectionTypePrevious = device->connectionType;
                if(rawInput->data.hid.dwSizeHid == 547)
                    device->connectionType = _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_BLUETOOTH;
                else if(rawInput->data.hid.dwSizeHid == 64)
                    device->connectionType = _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_USB;
                else
                {
                    device->connectionType = _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_UNKNOWN;
                    korl_log(ERROR, "invalid DS4 input report size: %u", rawInput->data.hid.dwSizeHid);
                    break;
                }
                korl_log(VERBOSE, "DS4 input reports: %u", rawInput->data.hid.dwCount);
                switch (device->connectionType)
                {
                case _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_BLUETOOTH:{
                    if(connectionTypePrevious == _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_UNKNOWN)
                        korl_log(INFO, "DS4 is now BLUETOOTH");
                    break;}
                case _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_USB:{
                    if(connectionTypePrevious == _KORL_WINDOWS_GAMEPAD_CONNECTIONTYPE_UNKNOWN)
                        korl_log(INFO, "DS4 is now USB");
                    break;}
                default: break;
                }
                break;}
            default:
                korl_log(ERROR, "unsupported RawInput device type: %u", device->type);
            }
            // korl_log(VERBOSE, "RawInput report: rawInput=0x%p bytes=%u", rawInput, rawInputBytes);
            /* process the input reports based on the device type */
            /* currently, we are assuming that DefWindowProc will be called for this message by our caller (korl-windows-window) */
            break;}
        case RIM_INPUTSINK:{// window is _not_ in foreground
            break;}
        }
        break;}
    case WM_INPUT_DEVICE_CHANGE:{
        const HANDLE handleDevice = KORL_C_CAST(HANDLE, lParam);
        switch(wParam)
        {
        case GIDC_ARRIVAL:{
            _Korl_Windows_Gamepad_Device* device                  = NULL;
            WCHAR*                        deviceNameAlignedBuffer = NULL;// allocated from temporary stack storage
            Korl_StringPool_String        stringDeviceName        = KORL_STRINGPOOL_STRING_NULL;// persistent; need to clean this up after allocation!
            /* determine how many characters the DEVICENAME contains */
            UINT deviceInfoSize = 0;
            UINT resultRawInput = GetRawInputDeviceInfo(handleDevice, RIDI_DEVICENAME, NULL, &deviceInfoSize);
            if(resultRawInput != 0)// calls to RawInput can fail at any time, as the device can suddenly become disconnected
            {
                korl_log(WARNING, "WM_INPUT_DEVICE_CHANGE: GIDC_ARRIVAL: get DEVICENAME size: GetRawInputDeviceInfo failed; GetLastError=0x%x", GetLastError());
                goto inputDeviceChange_arrival_cleanUp;
            }
            const u32 deviceNameSize = deviceInfoSize;// _including_ NULL-terminator
            /* obtain the DEVICENAME */
            deviceNameAlignedBuffer = korl_allocateAlignedDirty(context->allocatorHandleStack, deviceNameSize * sizeof(*deviceNameAlignedBuffer), RAW_INPUT_BUFFER_BYTE_ALIGNMENT);
            resultRawInput = GetRawInputDeviceInfo(handleDevice, RIDI_DEVICENAME, deviceNameAlignedBuffer, &deviceInfoSize);
            if(resultRawInput != deviceNameSize)// calls to RawInput can fail at any time, as the device can suddenly become disconnected
            {
                korl_log(WARNING, "WM_INPUT_DEVICE_CHANGE: GIDC_ARRIVAL: get DEVICENAME: GetRawInputDeviceInfo failed; GetLastError=0x%x", GetLastError());
                goto inputDeviceChange_arrival_cleanUp;
            }
            stringDeviceName = string_newUtf16(deviceNameAlignedBuffer);
            /* ensure that the DEVICENAME does not exist in our device database */
            {
                const _Korl_Windows_Gamepad_Device*const gamepadDeviceEnd = context->stbDaDevices + arrlen(context->stbDaDevices);
                for(_Korl_Windows_Gamepad_Device* gamepadDevice = context->stbDaDevices
                   ;gamepadDevice < gamepadDeviceEnd && !device
                   ;gamepadDevice++)
                    if(string_equals(&gamepadDevice->path, &stringDeviceName))
                        device = gamepadDevice;
            }
            /* obtain DEVICEINFO */
            KORL_ZERO_STACK(RID_DEVICE_INFO, deviceInfo);
            deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);// MSDN for GetRawInputDeviceInfo tells us to do this
            deviceInfoSize = sizeof(deviceInfo);
            resultRawInput = GetRawInputDeviceInfo(handleDevice, RIDI_DEVICEINFO, &deviceInfo, &deviceInfoSize);
            if(resultRawInput <= 0)
            {
                korl_log(WARNING, "WM_INPUT_DEVICE_CHANGE: GIDC_ARRIVAL: get DEVICEINFO: GetRawInputDeviceInfo failed; GetLastError=0x%x", GetLastError());
                goto inputDeviceChange_arrival_cleanUp;
            }
            /* determine what kind of device it is */
            _Korl_Windows_Gamepad_DeviceType deviceType = _KORL_WINDOWS_GAMEPAD_DEVICETYPE_INVALID;
            korl_shared_const DWORD ID_VENDOR_SONY = 0x054C;
            korl_shared_const DWORD ID_PRODUCT_DS4_GEN1 = 0x05C4;
            korl_shared_const DWORD ID_PRODUCT_DS4_GEN2 = 0x09CC;
            if(deviceInfo.dwType == RIM_TYPEHID)
            {
                // if XBOX; derived from MSDN recommended algorithm: https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput
                if(string_findUtf16(&stringDeviceName, L"IG_", 0) < deviceNameSize - 1/*StringPool returns length of String _excluding_ NULL-terminator on failure*/)
                    /* process XBOX devices using a different technique, since we can't get the data we need using RawInput */
                    deviceType = _KORL_WINDOWS_GAMEPAD_DEVICETYPE_INVALID;
                // if DS4
                else if(deviceInfo.hid.dwVendorId == ID_VENDOR_SONY
                        && (   deviceInfo.hid.dwProductId == ID_PRODUCT_DS4_GEN1
                            || deviceInfo.hid.dwProductId == ID_PRODUCT_DS4_GEN2))
                    deviceType = _KORL_WINDOWS_GAMEPAD_DEVICETYPE_DUALSHOCK4;
            }
            if(deviceType != _KORL_WINDOWS_GAMEPAD_DEVICETYPE_INVALID)
            {
                /* add a new gamepad to our database if necessary */
                korl_log(INFO, "WM_INPUT_DEVICE_CHANGE: GIDC_ARRIVAL: deviceName==\"%ws\" handle==%p", string_getRawUtf16(&stringDeviceName), handleDevice);
                if(!device)
                {
                    mcarrpush(KORL_STB_DS_MC_CAST(context->allocatorHandle), context->stbDaDevices, KORL_STRUCT_INITIALIZE_ZERO(_Korl_Windows_Gamepad_Device));
                    device = &arrlast(context->stbDaDevices);
                    device->path = stringDeviceName;
                    // take ownership of the path String so it doesn't get cleaned up at the end
                    korl_memory_zero(&stringDeviceName, sizeof(stringDeviceName));
                }
                device->type                 = deviceType;
                device->handleRawInputDevice = handleDevice;
                device->handleFileIo         = CreateFileW(string_getRawUtf16(&device->path), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if(device->handleFileIo == INVALID_HANDLE_VALUE)
                {
                    korl_log(WARNING, "failed to open device handle; reconnection required: \"%ws\"", string_getRawUtf16(&device->path));
                    break;
                }
            }
            inputDeviceChange_arrival_cleanUp:
                string_free(&stringDeviceName);
            break;}
        case GIDC_REMOVAL:{
            /* for the REMOVAL event, handleDevice has already been invalidated, so we can't call RawInput APIs on it; 
                we can only check to see if it matches any previously registered device handles & act appropriately */
            _Korl_Windows_Gamepad_Device* device = NULL;
            const _Korl_Windows_Gamepad_Device*const gamepadDeviceEnd = context->stbDaDevices + arrlen(context->stbDaDevices);
            for(_Korl_Windows_Gamepad_Device* gamepadDevice = context->stbDaDevices
               ;gamepadDevice < gamepadDeviceEnd && !device
               ;gamepadDevice++)
                if(gamepadDevice->handleRawInputDevice == handleDevice)
                    device = gamepadDevice;
            if(!device)
                break;
            korl_log(INFO, "WM_INPUT_DEVICE_CHANGE: GIDC_REMOVAL: handle==%p", handleDevice);
            _korl_windows_gamepad_device_disconnect(device);
            break;}
        default: korl_log(ERROR, "WM_INPUT_DEVICE_CHANGE: unexpected wParam: %llu", wParam);
        }
        *out_result = 0;
        break;}
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
                Korl_StringPool_String stringDevicePath = string_newUtf16(deviceInterface->dbcc_name);
                string_toUpper(stringDevicePath);// necessary since apparently SetupDi* API & WM_DEVICECHANGE message structures provide different path cases
                const _Korl_Windows_Gamepad_Device*const devicesEnd = context->stbDaDevices + arrlen(context->stbDaDevices);
                for(_Korl_Windows_Gamepad_Device* device = context->stbDaDevices
                   ;device < devicesEnd
                   ;device++)
                    /* if a device in our database matches the devicePath, we need to clean it up and remove it */
                    if(string_equals(&stringDevicePath, &device->path))
                    {
                        _korl_windows_gamepad_device_disconnect(device);
                        goto deviceChange_removeComplete_cleanUp;
                    }
                korl_log(WARNING, "device disconnected, but not present in database: \"%ws\"", string_getRawUtf16(&stringDevicePath));
                deviceChange_removeComplete_cleanUp:
                    string_free(&stringDevicePath);
            }
            else
                out_result = NULL;
            break;}
        }
        if(out_result)
            *out_result = 0;
        break;}
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
    const _Korl_Windows_Gamepad_Device*const gamepadDeviceEnd = context->stbDaDevices + arrlen(context->stbDaDevices);
    for(_Korl_Windows_Gamepad_Device* device = context->stbDaDevices
       ;device < gamepadDeviceEnd
       ;device++)
    {
        if(device->type != _KORL_WINDOWS_GAMEPAD_DEVICETYPE_XINPUT)
            continue;// only support XBOX gamepads (for now)
        if(device->handleFileIo == INVALID_HANDLE_VALUE)
            continue;// the device is disconnected
        BYTE in[3] = { 0x01, 0x01, 0x00 };//KORL-ISSUE-000-000-083: gamepad: XBOX wireless devices only support a single gamepad instead of 4
        BYTE out[29];
        DWORD size;
        if(   !DeviceIoControl(device->handleFileIo, IOCTL_XUSB_GET_STATE, in, sizeof(in), out, sizeof(out), &size, NULL) 
           || size != sizeof(out))
        {
            switch(GetLastError())
            {
            case ERROR_BAD_COMMAND:{// device is disconnected; no documentation on this (classic M$), and this nomenclature makes no sense, but it happens empirically
                _korl_windows_gamepad_device_disconnect(device);
                continue;}
            case ERROR_DEVICE_NOT_CONNECTED:{
                /* We can't just disconnect the XBOX controller device here, since 
                    it is entirely possible that this is a wireless controller receiver, 
                    which can communicate with multiple controllers. */
                // korl_log(INFO, "DeviceIoControl => ERROR_DEVICE_NOT_CONNECTED; disconnecting gamepad...");
                continue;}
            // KORL-ISSUE-000-000-164: gamepad: handle ERROR_BAD_COMMAND case (when I unplug controller?)
            default: {
                korl_logLastError("DeviceIoControl failed");
                continue;}
            }
        }
        const _Korl_Windows_Gamepad_Device deviceStatePrevious = *device;
        korl_assert(korl_memory_isLittleEndian());// the raw xbox state data is stored in the buffer in little-endian byte order; so we can only C-cast into multi-byte registers if our platform is also little-endian
        device->lastState.xbox.inputFrame           = *KORL_C_CAST(DWORD*, out +  5);
        device->lastState.xbox.buttons              = *KORL_C_CAST(WORD* , out + 11);
        device->lastState.xbox.sticks.named.leftX   = *KORL_C_CAST(SHORT*, out + 15);
        device->lastState.xbox.sticks.named.leftY   = *KORL_C_CAST(SHORT*, out + 17);
        device->lastState.xbox.sticks.named.rightX  = *KORL_C_CAST(SHORT*, out + 19);
        device->lastState.xbox.sticks.named.rightY  = *KORL_C_CAST(SHORT*, out + 21);
        device->lastState.xbox.triggers.named.left  = out[13];
        device->lastState.xbox.triggers.named.right = out[14];
        /** deadzone filter */
        //KORL-FEATURE-000-000-036: gamepad: (low priority); expose gamepad deadzone values as configurable through the KORL platform interface
        /* these deadzone values are derived from: 
            https://docs.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput#dead-zone */
        #define _KORL_XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
        #define _KORL_XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
        #define _KORL_XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30
        _korl_windows_gamepad_xInputFilterStickDeadzone(&device->lastState.xbox.sticks.named.leftX
                                                       ,&device->lastState.xbox.sticks.named.leftY
                                                       ,_KORL_XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        _korl_windows_gamepad_xInputFilterStickDeadzone(&device->lastState.xbox.sticks.named.rightX
                                                       ,&device->lastState.xbox.sticks.named.rightY
                                                       ,_KORL_XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        _korl_windows_gamepad_xInputFilterTriggerDeadzone(&device->lastState.xbox.triggers.named.left
                                                         ,_KORL_XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        _korl_windows_gamepad_xInputFilterTriggerDeadzone(&device->lastState.xbox.triggers.named.right
                                                         ,_KORL_XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        /** these flags are presented in the exact order that the corresponding 
         * KORL button value in the Korl_GamepadButton enumeration; it is 
         * essentially a mapping from the xbox_button_bitfield => KORL_button_index */
        korl_shared_const WORD XBOX_BUTTON_FLAGS[] = 
            { 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080
            , 0x0100, 0x0200, 0x0400,         0x1000, 0x2000, 0x4000, 0x8000 };
        //KORL-ISSUE-000-000-084: gamepad: (low priority) XBOX controller guide button is not being captured
        for(u$ b = 0; b < korl_arraySize(XBOX_BUTTON_FLAGS); b++)
        {
            if(   (            device->lastState.xbox.buttons & XBOX_BUTTON_FLAGS[b])
               == (deviceStatePrevious.lastState.xbox.buttons & XBOX_BUTTON_FLAGS[b]))
                continue;
            if(!onGamepadEvent)
                continue;
            onGamepadEvent((Korl_GamepadEvent){.type = KORL_GAMEPAD_EVENT_TYPE_BUTTON
                                              ,.subType = {.button = {.index   = KORL_C_CAST(Korl_GamepadButton, b)
                                                                     ,.pressed = device->lastState.xbox.buttons & XBOX_BUTTON_FLAGS[b]}}});
        }
        for(u$ a = 0; a < korl_arraySize(deviceStatePrevious.lastState.xbox.sticks.array); a++)
        {
            if(device->lastState.xbox.sticks.array[a] != deviceStatePrevious.lastState.xbox.sticks.array[a])
                if(onGamepadEvent)
                    onGamepadEvent((Korl_GamepadEvent){.type = KORL_GAMEPAD_EVENT_TYPE_AXIS
                                                      ,.subType = {.axis = {.index = KORL_C_CAST(Korl_GamepadAxis, a)
                                                                           ,.value = KORL_C_CAST(f32, device->lastState.xbox.sticks.array[a]) / KORL_I16_MAX}}});
        }
        for(u$ t = 0; t < korl_arraySize(deviceStatePrevious.lastState.xbox.triggers.array); t++)
        {
            if(device->lastState.xbox.triggers.array[t] != deviceStatePrevious.lastState.xbox.triggers.array[t])
                if(onGamepadEvent)
                    onGamepadEvent((Korl_GamepadEvent){.type = KORL_GAMEPAD_EVENT_TYPE_AXIS
                                                      ,.subType = {.axis = {.index = KORL_C_CAST(Korl_GamepadAxis, t + KORL_GAMEPAD_AXIS_TRIGGER_LEFT)
                                                                           ,.value = KORL_C_CAST(f32, device->lastState.xbox.triggers.array[t]) / KORL_U8_MAX}}});
        }
    }
}
#undef _LOCAL_STRING_POOL_POINTER
