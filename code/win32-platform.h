#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "win32-network.h"
#include "win32-jobQueue.h"
#include "win32-xinput.h"
#include "stb/stb_vorbis.h"
global_variable stb_vorbis_alloc g_oggVorbisAlloc;
global_variable TCHAR g_pathToAssets[MAX_PATH];
global_variable JobQueue g_jobQueue;
global_variable KgaHandle g_hKgaRawFiles;
global_variable CRITICAL_SECTION g_csLockAllocatorRawFiles;
global_variable CRITICAL_SECTION g_assetAllocationCsLock;
global_variable CRITICAL_SECTION g_vorbisAllocationCsLock;
global_variable CRITICAL_SECTION g_logCsLock;
global_variable bool g_hasReceivedException;
global_variable bool g_displayCursor = true;
global_variable bool g_mouseRelativeMode;
global_variable POINT g_mouseRelativeModeCachedScreenPosition;
global_variable LARGE_INTEGER g_perfCounterHz;
// Remember, there are two log buffers: one beginning & a circular buffer.  So,
//	the total # of characters used for logging is 2*MAX_LOG_BUFFER_SIZE.
global_variable const size_t MAX_LOG_BUFFER_SIZE = 32768;
global_variable char g_logBeginning[MAX_LOG_BUFFER_SIZE];
global_variable char g_logCircularBuffer[MAX_LOG_BUFFER_SIZE] = {};
global_variable size_t g_logCurrentBeginningChar;
global_variable size_t g_logCurrentCircularBufferChar;
// This represents the total # of character sent to the circular buffer, 
//	including characters that are overwritten!
global_variable size_t g_logCircularBufferRunningTotal;
global_variable GamePad g_gamePadArrayA[XUSER_MAX_COUNT + 
                                            CARRAY_SIZE(g_dInputDevices)] = {};
global_variable GamePad g_gamePadArrayB[XUSER_MAX_COUNT + 
                                            CARRAY_SIZE(g_dInputDevices)] = {};
struct Win32LockResource
{
	bool allocated;
	CRITICAL_SECTION csLock;
};
global_variable Win32LockResource g_platformExposedLocks[8];
internal PLATFORM_IS_FULLSCREEN(w32PlatformIsFullscreen);
internal PLATFORM_SET_FULLSCREEN(w32PlatformSetFullscreen);
internal PLATFORM_POST_JOB(w32PlatformPostJob);
internal PLATFORM_JOB_DONE(w32PlatformJobDone);
internal PLATFORM_JOB_VALID(w32PlatformJobValid);
internal PLATFORM_LOAD_PNG(w32PlatformLoadPng);
internal PLATFORM_LOAD_OGG(w32PlatformLoadOgg);
internal PLATFORM_LOAD_WAV(w32PlatformLoadWav);
internal PLATFORM_IMGUI_ALLOC(w32PlatformImguiAlloc);
internal PLATFORM_IMGUI_FREE(w32PlatformImguiFree);
internal PLATFORM_DECODE_Z85_PNG(w32PlatformDecodeZ85Png);
internal PLATFORM_DECODE_Z85_WAV(w32PlatformDecodeZ85Wav);
internal PLATFORM_LOG(w32PlatformLog);
internal PLATFORM_ASSERT(w32PlatformAssert);
internal PLATFORM_GET_ASSET_BYTE_SIZE(w32PlatformGetAssetByteSize);
internal PLATFORM_READ_ENTIRE_ASSET(w32PlatformReadEntireAsset);
internal PLATFORM_WRITE_ENTIRE_FILE(w32PlatformWriteEntireFile);
internal PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(w32PlatformGetGamePadActiveButton);
internal PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(w32PlatformGetGamePadActiveAxis);
internal PLATFORM_GET_GAME_PAD_PRODUCT_NAME(w32PlatformGetGamePadProductName);
internal PLATFORM_GET_GAME_PAD_PRODUCT_GUID(w32PlatformGetGamePadProductGuid);
internal PLATFORM_GET_ASSET_WRITE_TIME(w32PlatformGetAssetWriteTime);
internal PLATFORM_IS_ASSET_CHANGED(w32PlatformIsAssetChanged);
internal PLATFORM_IS_ASSET_AVAILABLE(w32PlatformIsAssetAvailable);
internal PLATFORM_GET_TIMESTAMP(w32PlatformGetTimeStamp);
internal PLATFORM_SECONDS_SINCE_TIMESTAMP(w32PlatformSecondsSinceTimeStamp);
internal PLATFORM_SLEEP_FROM_TIMESTAMP(w32PlatformSleepFromTimeStamp);
internal PLATFORM_RESERVE_LOCK(w32PlatformReserveLock);
internal PLATFORM_LOCK(w32PlatformLock);
internal PLATFORM_UNLOCK(w32PlatformUnlock);
internal PLATFORM_MOUSE_SET_HIDDEN(w32PlatformMouseSetHidden);
internal PLATFORM_MOUSE_SET_RELATIVE_MODE(w32PlatformMouseSetRelativeMode);
const global_variable KmlPlatformApi KML_PLATFORM_API_WIN32 = 
	{ .postJob                = w32PlatformPostJob
	, .jobValid               = w32PlatformJobValid
	, .jobDone                = w32PlatformJobDone
	, .log                    = w32PlatformLog
	, .assert                 = w32PlatformAssert
	, .decodeZ85Png           = w32PlatformDecodeZ85Png
	, .decodeZ85Wav           = w32PlatformDecodeZ85Wav
	, .getAssetByteSize       = w32PlatformGetAssetByteSize
	, .readEntireAsset        = w32PlatformReadEntireAsset
	, .loadWav                = w32PlatformLoadWav
	, .loadOgg                = w32PlatformLoadOgg
	, .loadPng                = w32PlatformLoadPng
	, .getAssetWriteTime      = w32PlatformGetAssetWriteTime
	, .isAssetChanged         = w32PlatformIsAssetChanged
	, .isAssetAvailable       = w32PlatformIsAssetAvailable
	, .isFullscreen           = w32PlatformIsFullscreen
	, .setFullscreen          = w32PlatformSetFullscreen
	, .getGamePadActiveButton = w32PlatformGetGamePadActiveButton
	, .getGamePadActiveAxis   = w32PlatformGetGamePadActiveAxis
	, .getGamePadProductName  = w32PlatformGetGamePadProductName
	, .getGamePadProductGuid  = w32PlatformGetGamePadProductGuid
	, .getTimeStamp           = w32PlatformGetTimeStamp
	, .sleepFromTimeStamp     = w32PlatformSleepFromTimeStamp
	, .secondsSinceTimeStamp  = w32PlatformSecondsSinceTimeStamp
	, .netResolveAddress      = w32PlatformNetworkResolveAddress
	, .socketOpenUdp          = w32PlatformNetworkOpenSocketUdp
	, .socketClose            = w32PlatformNetworkCloseSocket
	, .socketSend             = w32PlatformNetworkSend
	, .socketReceive          = w32PlatformNetworkReceive
	, .reserveLock            = w32PlatformReserveLock
	, .lock                   = w32PlatformLock
	, .unlock                 = w32PlatformUnlock
	, .mouseSetHidden         = w32PlatformMouseSetHidden
	, .mouseSetRelativeMode   = w32PlatformMouseSetRelativeMode
};