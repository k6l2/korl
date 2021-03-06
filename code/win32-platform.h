#pragma once
#include "kutil.h"
#include "platform-game-interfaces.h"
#include "win32-network.h"
#include "win32-jobQueue.h"
#include "win32-xinput.h"
#include "stb/stb_vorbis.h"
global_variable TCHAR g_pathCurrentDirectory[MAX_PATH];
global_variable stb_vorbis_alloc g_oggVorbisAlloc;
global_variable JobQueue g_jobQueue;
global_variable CRITICAL_SECTION g_vorbisAllocationCsLock;
global_variable CRITICAL_SECTION g_logCsLock;
global_variable bool g_hasReceivedException;
global_variable bool g_displayCursor = true;
global_variable bool g_mouseRelativeMode;
global_variable POINT g_mouseRelativeModeCachedScreenPosition;
global_variable GamePad g_gamePadArrayA[XUSER_MAX_COUNT + 
                                            CARRAY_SIZE(g_dInputDevices)] = {};
global_variable GamePad g_gamePadArrayB[XUSER_MAX_COUNT + 
                                            CARRAY_SIZE(g_dInputDevices)] = {};
global_variable bool g_echoLogToDebug = true;
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
internal PLATFORM_IMGUI_ALLOC(w32PlatformImguiAlloc);
internal PLATFORM_IMGUI_FREE(w32PlatformImguiFree);
internal PLATFORM_DECODE_PNG(w32PlatformDecodePng);
internal PLATFORM_DECODE_AUDIO_FILE(w32PlatformDecodeAudioFile);
internal PLATFORM_LOG(w32PlatformLog);
internal KORL_PLATFORM_ASSERT_FAILURE(w32PlatformAssertFailure);
internal PLATFORM_GET_FILE_HANDLE(w32PlatformGetFileHandle);
internal PLATFORM_RELEASE_FILE_HANDLE(w32PlatformReleaseFileHandle);
internal PLATFORM_GET_FILE_WRITE_TIME(w32PlatformGetFileWriteTime);
internal PLATFORM_IS_FILE_CHANGED(w32PlatformIsFileChanged);
internal PLATFORM_GET_FILE_BYTE_SIZE(w32PlatformGetFileByteSize);
internal PLATFORM_READ_ENTIRE_FILE(w32PlatformReadEntireFile);
internal PLATFORM_WRITE_ENTIRE_FILE(w32PlatformWriteEntireFile);
internal PLATFORM_CREATE_DIRECTORY(w32PlatformCreateDirectory);
internal PLATFORM_GET_DIRECTORY_ENTRIES(w32PlatformGetDirectoryEntries);
internal PLATFORM_DESTROY_DIRECTORY_ENTRY(w32PlatformDestroyDirectoryEntry);
internal PLATFORM_RENAME_DIRECTORY_ENTRY(w32PlatformRenameDirectoryEntry);
internal PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(w32PlatformGetGamePadActiveButton);
internal PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(w32PlatformGetGamePadActiveAxis);
internal PLATFORM_GET_GAME_PAD_PRODUCT_NAME(w32PlatformGetGamePadProductName);
internal PLATFORM_GET_GAME_PAD_PRODUCT_GUID(w32PlatformGetGamePadProductGuid);
internal PLATFORM_GET_TIMESTAMP(w32PlatformGetTimeStamp);
internal PLATFORM_SECONDS_SINCE_TIMESTAMP(w32PlatformSecondsSinceTimeStamp);
internal PLATFORM_SECONDS_BETWEEN_TIMESTAMPS(
	w32PlatformSecondsBetweenTimeStamps);
internal PLATFORM_MICROSECONDS_BETWEEN_TIMESTAMPS(
	w32PlatformMicroSecondsBetweenTimeStamps);
internal PLATFORM_GET_DATESTAMP(w32PlatformGetDateStamp);
internal PLATFORM_GENERATE_DATESTAMP_STRING(w32PlatformGenerateDateStampString);
internal PLATFORM_SLEEP_FROM_TIMESTAMP(w32PlatformSleepFromTimeStamp);
internal PLATFORM_RESERVE_LOCK(w32PlatformReserveLock);
internal PLATFORM_LOCK(w32PlatformLock);
internal PLATFORM_UNLOCK(w32PlatformUnlock);
internal PLATFORM_MOUSE_SET_HIDDEN(w32PlatformMouseSetHidden);
internal PLATFORM_MOUSE_SET_RELATIVE_MODE(w32PlatformMouseSetRelativeMode);
internal PLATFORM_ENUMERATE_WINDOWS(w32PlatformEnumerateWindows);
internal PLATFORM_GET_WINDOW_RAW_IMAGE(w32PlatformGetWindowRawImage);
const global_variable KorlPlatformApi KORL_PLATFORM_API_WIN32 = 
	{ .postJob                       = w32PlatformPostJob
	, .jobValid                      = w32PlatformJobValid
	, .jobDone                       = w32PlatformJobDone
	, .log                           = w32PlatformLog
	, .assertFailure                 = w32PlatformAssertFailure
	, .decodePng                     = w32PlatformDecodePng
	, .decodeAudioFile               = w32PlatformDecodeAudioFile
	, .getFileHandle                 = w32PlatformGetFileHandle
	, .releaseFileHandle             = w32PlatformReleaseFileHandle
	, .getFileWriteTime              = w32PlatformGetFileWriteTime
	, .isFileChanged                 = w32PlatformIsFileChanged
	, .getFileByteSize               = w32PlatformGetFileByteSize
	, .readEntireFile                = w32PlatformReadEntireFile
	, .writeEntireFile               = w32PlatformWriteEntireFile
	, .createDirectory               = w32PlatformCreateDirectory
	, .getDirectoryEntries           = w32PlatformGetDirectoryEntries
	, .destroyDirectoryEntry         = w32PlatformDestroyDirectoryEntry
	, .renameDirectoryEntry          = w32PlatformRenameDirectoryEntry
	, .isFullscreen                  = w32PlatformIsFullscreen
	, .setFullscreen                 = w32PlatformSetFullscreen
	, .getGamePadActiveButton        = w32PlatformGetGamePadActiveButton
	, .getGamePadActiveAxis          = w32PlatformGetGamePadActiveAxis
	, .getGamePadProductName         = w32PlatformGetGamePadProductName
	, .getGamePadProductGuid         = w32PlatformGetGamePadProductGuid
	, .getTimeStamp                  = w32PlatformGetTimeStamp
	, .sleepFromTimeStamp            = w32PlatformSleepFromTimeStamp
	, .secondsSinceTimeStamp         = w32PlatformSecondsSinceTimeStamp
	, .secondsBetweenTimeStamps      = w32PlatformSecondsBetweenTimeStamps
	, .microSecondsBetweenTimeStamps = w32PlatformMicroSecondsBetweenTimeStamps
	, .getDateStamp                  = w32PlatformGetDateStamp
	, .generateDateStampString       = w32PlatformGenerateDateStampString
	, .netResolveAddress             = w32PlatformNetworkResolveAddress
	, .socketOpenUdp                 = w32PlatformNetworkOpenSocketUdp
	, .socketClose                   = w32PlatformNetworkCloseSocket
	, .socketSend                    = w32PlatformNetworkSend
	, .socketReceive                 = w32PlatformNetworkReceive
	, .reserveLock                   = w32PlatformReserveLock
	, .lock                          = w32PlatformLock
	, .unlock                        = w32PlatformUnlock
	, .mouseSetHidden                = w32PlatformMouseSetHidden
	, .mouseSetRelativeMode          = w32PlatformMouseSetRelativeMode
	, .enumerateWindows              = w32PlatformEnumerateWindows
	, .getWindowRawImage             = w32PlatformGetWindowRawImage
};
