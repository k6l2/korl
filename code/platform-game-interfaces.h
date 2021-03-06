#pragma once
#include "kutil.h"
#include "krb-interface.h"
#include "kgtAllocator.h"
#include "kmath.h"
//#include <immintrin.h> // AVX, AVX2, FMA
//#include <zmmintrin.h> // AVX512?
/* define hardware-specific intrinsics (AVX, etc...) */
/* @todo: define hardware-specific intrinsics for non-windows platforms */
#if defined(_MSC_VER)
	#include <intrin.h>
#endif
// Data structures which must be the same for the Platform & Game layers ///////
/* PLATFORM INTERFACE *********************************************************/
struct RawImage
{
	u32 sizeX;
	u32 sizeY;
	/** If this member is 0, then each row of pixelData is fully packed.  
	 * Otherwise, there is a non-zero amount of padding at the end of each row 
	 * 	and therefore, the TOTAL BYTE SIZE of a row of pixelData is indicated by 
	 * 	this value!  For example, if a RawImage has a sizeX==7, and 
	 * 	pixelDataFormat==BGR, and rowByteStride==28, then a single row of this 
	 * 	RawImage will consist of 7 BGR pixels 
	 * 	(7_pixels * 3_bytes_per_pixel == 21_bytes), followed by 7 bytes of 
	 * 	unused padding! */
	u32 rowByteStride;
	KorlPixelDataFormat pixelDataFormat;
	/* NOTE: when interpreting pixels as u32 values, color order depends on the 
			endian-ness of the system!  USE THE HELPER FUNCTIONS DECLARED BELOW 
			for pixel access if you want to remain sane. */
	u8* pixelData;
};
internal u8 
	korlRawImageGetRed(const RawImage& rawImg, u64 pixelIndex);
internal u8 
	korlRawImageGetRed(const RawImage& rawImg, const v2u32& coord);
internal u8 
	korlRawImageGetGreen(const RawImage& rawImg, u64 pixelIndex);
internal u8 
	korlRawImageGetGreen(const RawImage& rawImg, const v2u32& coord);
internal u8 
	korlRawImageGetBlue(const RawImage& rawImg, u64 pixelIndex);
internal u8 
	korlRawImageGetBlue(const RawImage& rawImg, const v2u32& coord);
internal u8 
	korlRawImageGetAlpha(const RawImage& rawImg, u64 pixelIndex);
internal u8 
	korlRawImageGetAlpha(const RawImage& rawImg, const v2u32& coord);
internal void 
	korlRawImageSetPixel(RawImage* rawImg, u64 pixelIndex, u8 r, u8 g, u8 b);
internal void 
	korlRawImageSetPixel(
		RawImage* rawImg, const v2u32& coord, u8 r, u8 g, u8 b);
struct RawSound
{
	u32 sampleHz;
	u32 sampleBlockCount;
	u16 bitsPerSample;
	u8 channelCount;
	u8 channelCount_PADDING[5];
	// A sample block is a contiguous collection of all samples from all 
	//	channels.  Sample data is stored in blocks.
	SoundSample* sampleData;
};
using FileWriteTime = u64;
/** Do not use this value directly, since the meaning of this data is 
 * platform-dependent.  Instead, use the platform APIs which uses this type.  
 * This value should represent the highest possible time resolution 
 * (microseconds) of the platform, but it should only be valid for the current 
 * session of the underlying hardware.  (Do NOT save this value to disk and then 
 * expect it to be correct when you load it from disk on another 
 * session/machine!) */
using PlatformTimeStamp = u64;
/** Do not use this value directly, since the meaning of this data is 
 * platform-dependent.  Instead, use the platform APIs which uses this type.  
 * This value should be considered lower resolution (milliseconds) than 
 * PlatformTimeStamp, but is synchronized to a machine-independent time 
 * reference (UTC), so it should remain valid between application runs. */
using PlatformDateStamp = u64;
using JobQueueTicket = u32;
#define JOB_QUEUE_FUNCTION(name) \
	void name(void* data, u32 threadId)
typedef JOB_QUEUE_FUNCTION(fnSig_jobQueueFunction);
/** 
 * @return the job ticket which uses `function`.  If `function` is nullptr, the 
 *         return value is guaranteed to be an INVALID ticket.
*/
#define PLATFORM_POST_JOB(name) JobQueueTicket name(\
	fnSig_jobQueueFunction* function, void* data)
#define PLATFORM_JOB_VALID(name) bool name(JobQueueTicket* ticket)
#define PLATFORM_JOB_DONE(name) bool name(JobQueueTicket* ticket)
#define PLATFORM_IMGUI_ALLOC(name) void* name(size_t sz, void* user_data)
#define PLATFORM_IMGUI_FREE(name) void  name(void* ptr, void* user_data)
#define KORL_CALLBACK_REQUEST_MEMORY(name) void* name(\
	u32 requestedByteCount, void* userData)
typedef KORL_CALLBACK_REQUEST_MEMORY(fnSig_korlCallbackRequestMemory);
/** Caller is responsible for keeping whatever allocators used inside 
 * `requestMemoryPixelData` thread-safe! 
 * @return 
 * If there is a failure loading the file, an invalid RawImage where 
 * `pixelData == nullptr` is returned. */
#define PLATFORM_DECODE_PNG(name) RawImage name(\
	const void* data, u32 dataBytes, \
	fnSig_korlCallbackRequestMemory* requestMemoryPixelData, \
	void* requestMemoryPixelDataUserData)
enum class KorlAudioFileType : u8
	{ WAVE
	, OGG_VORBIS };
#define PLATFORM_DECODE_AUDIO_FILE(name) RawSound name(\
	const u8* data, u32 dataBytes, \
	fnSig_korlCallbackRequestMemory* requestMemorySampleData, \
	void* requestMemorySampleDataUserData, KorlAudioFileType dataFileType)
/* @todo: decode oggvorbis */
enum class KorlApplicationDirectory : u8
	{ CURRENT     // same directory as the platform executable; assume READ-ONLY
	, LOCAL       // platform-defined; READ + WRITE access, safe storage!
	, TEMPORARY };// platform-defined; READ + WRITE access, LOSSY storage!
enum class KorlFileHandleUsage : u8
	{ READ_EXISTING
	, WRITE_NEW };
using KorlFileHandle = void*;
/** Obtain an opaque handle to a file.  Upon success, the caller can be assured 
 * that it has exclusive access to the file, and they can proceed to call KORL 
 * API which use this file handle without worrying about whether or not the file 
 * has been modified by another process.
 * @return
 * true if o_hFile contains a valid file handle, false otherwise */
#define PLATFORM_GET_FILE_HANDLE(name) bool name(\
	const char* ansiFilePath, KorlApplicationDirectory pathOrigin, \
	KorlFileHandleUsage usage, KorlFileHandle* o_hFile)
#define PLATFORM_RELEASE_FILE_HANDLE(name) void name(\
	KorlFileHandle hFile)
#define PLATFORM_GET_FILE_WRITE_TIME(name) FileWriteTime name(\
	const char* ansiFilePath, KorlApplicationDirectory pathOrigin)
#define PLATFORM_IS_FILE_CHANGED(name) bool name(\
	const char* ansiFilePath, KorlApplicationDirectory pathOrigin, \
	FileWriteTime lastWriteTime)
/** @return a value < 0 if an error occurs */
#define PLATFORM_GET_FILE_BYTE_SIZE(name) i32 name(KorlFileHandle hFile)
#define PLATFORM_READ_ENTIRE_FILE(name) bool name(\
	KorlFileHandle hFile, void* o_data, u32 dataBytes)
#define PLATFORM_WRITE_ENTIRE_FILE(name) bool name(\
	KorlFileHandle hFile, const void* data, u32 dataBytes)
#define PLATFORM_CREATE_DIRECTORY(name) bool name(\
	const char*const ansiDirectoryPath, KorlApplicationDirectory pathOrigin)
#define KORL_CALLBACK_DIRECTORY_ENTRY_FOUND(name) void name(\
	const char*const ansiEntryName, const char*const ansiDirectoryPath, \
	KorlApplicationDirectory pathOrigin, bool isFile, bool isDirectory, \
	void* userData)
typedef KORL_CALLBACK_DIRECTORY_ENTRY_FOUND(
	fnSig_korlCallbackDirectoryEntryFound);
/**
 * @return false if the `pathOrigin`+`ansiDirectoryPath` directory doesn't exist
 */
#define PLATFORM_GET_DIRECTORY_ENTRIES(name) bool name(\
	const char*const ansiDirectoryPath, KorlApplicationDirectory pathOrigin, \
	fnSig_korlCallbackDirectoryEntryFound* callbackEntryFound, \
	void* callbackEntryFoundUserData)
#define PLATFORM_DESTROY_DIRECTORY_ENTRY(name) bool name(\
	const char*const ansiDirectoryEntryPath, \
	KorlApplicationDirectory pathOrigin)
#define PLATFORM_RENAME_DIRECTORY_ENTRY(name) bool name(\
	const char*const ansiDirectoryEntryPath, \
	KorlApplicationDirectory pathOrigin, \
	const char*const ansiDirectoryEntryPathNew)
#define PLATFORM_IS_FULLSCREEN(name) bool name()
global_variable const u16 INVALID_PLATFORM_BUTTON_INDEX = u16(~0);
#define PLATFORM_SET_FULLSCREEN(name) void name(bool isFullscreenDesired)
/** Don't use this API for anything related to game development!!!  The only 
 * reason this exists is to discover hardware-specific controller input maps. 
 * @return 
 * INVALID_PLATFORM_BUTTON_INDEX if there is no active button OR if there are 
 * more than one buttons active! */
#define PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(name) u16 name(u8 gamePadIndex)
global_variable const u16 INVALID_PLATFORM_AXIS_INDEX = u16(~0);
struct PlatformGamePadActiveAxis
{
	u16 index;
	bool positive;
};
/** Don't use this API for anything related to game development!!!  The only 
 * reason this exists is to discover hardware-specific controller input maps. 
 * @return INVALID_PLATFORM_AXIS_INDEX if there is no active axis OR if there 
 *         are more than one axes active!
 */
#define PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(name) PlatformGamePadActiveAxis name(\
	u8 gamePadIndex)
/** Don't use this API for anything related to game development!!!  The only 
 * reason this exists is to discover hardware-specific controller input maps. 
 */
#define PLATFORM_GET_GAME_PAD_PRODUCT_NAME(name) void name(\
	u8 gamePadIndex, char* o_buffer, size_t bufferSize)
/** Don't use this API for anything related to game development!!!  The only 
 * reason this exists is to discover hardware-specific controller input maps. 
 */
#define PLATFORM_GET_GAME_PAD_PRODUCT_GUID(name) void name(\
	u8 gamePadIndex, char* o_buffer, size_t bufferSize)
#define PLATFORM_GET_TIMESTAMP(name) PlatformTimeStamp name()
#define PLATFORM_SLEEP_FROM_TIMESTAMP(name) void name(\
	PlatformTimeStamp pts, f32 desiredDeltaSeconds)
#define PLATFORM_SECONDS_SINCE_TIMESTAMP(name) f32 name(PlatformTimeStamp pts)
/** It doesn't matter what order the PlatformTimeStamp parameters are in. */
#define PLATFORM_SECONDS_BETWEEN_TIMESTAMPS(name) f32 name(\
	PlatformTimeStamp ptsA, PlatformTimeStamp ptsB)
/** It doesn't matter what order the PlatformTimeStamp parameters are in. */
#define PLATFORM_MICROSECONDS_BETWEEN_TIMESTAMPS(name) u64 name(\
	PlatformTimeStamp ptsA, PlatformTimeStamp ptsB)
#define PLATFORM_GET_DATESTAMP(name) PlatformDateStamp name()
/**
 * @param cStrBufferSize 
 * 	A size of 32 is recommended.  Current implementation prints out the maximum 
 * 	of 23 characters to represent the local time (yyyy-MM-dd-hh:mm'ss"uuu) plus 
 * 	the null-terminator character, bringing the total to 24.
 * @return the # of characters written to `o_cStrBuffer`
 */
#define PLATFORM_GENERATE_DATESTAMP_STRING(name) u32 name(\
	PlatformDateStamp pds, char*const o_cStrBuffer, u32 cStrBufferSize)
/* IPv4 UDP datagrams cannot be larger than this amount.  Source:
https://en.wikipedia.org/wiki/User_Datagram_Protocol#:~:text=The%20field%20size%20sets%20a,%E2%88%92%2020%20byte%20IP%20header). */
const global_variable u32 KPL_MAX_DATAGRAM_SIZE = 65507;
using KplSocketIndex = u8;
const global_variable KplSocketIndex 
	KPL_INVALID_SOCKET_INDEX = KplSocketIndex(~0);
union KplNetAddress
{
	/* 16 bytes is enough to store an IPv6 address.  IPv4 addresses only use 4 
		bytes, but who cares?  Memory is free.™ */
	u8 uBytes[16];
	u32 uInts[4];
	u64 uLongs[2];
};
internal inline bool operator==(const KplNetAddress& lhs, 
                                const KplNetAddress& rhs)
{
	return lhs.uLongs[0] == rhs.uLongs[0] && lhs.uLongs[1] == rhs.uLongs[1];
}
const global_variable KplNetAddress KPL_INVALID_ADDRESS = {};
/** This function often takes a long time to complete, so it's probably best to 
 * run this as an asynchronous job (see PLATFORM_POST_JOB). */
#define PLATFORM_NET_RESOLVE_ADDRESS(name) \
	KplNetAddress name(const char* ansiAddress)
/** 
 * @param listenPort If this is 0, the port which the socket listens on is 
 *                   chosen by the platform automatically.  This value does not 
 *                   prevent the user application from SENDING to other ports.
 * @param address if this is nullptr, the socket is bound to ANY address
 * @return KPL_INVALID_SOCKET_INDEX if the socket could not be created for any 
 *         reason.  On success, an opaque index to a unique resource 
 *         representing a non-blocking UDP socket bound to `listenPort` is 
 *         returned.
*/
#define PLATFORM_SOCKET_OPEN_UDP(name) KplSocketIndex name(u16 listenPort)
#define PLATFORM_SOCKET_CLOSE(name) void name(KplSocketIndex socketIndex)
/**
 * @return the # of bytes sent.  If an error occurs, a value < 0 is returned.  
 *         If the socket or the underlying platform networking implementation is 
 *         not ready to send right now, 0 is returned (no errors occurred)
 */
#define PLATFORM_SOCKET_SEND(name) i32 name(\
	KplSocketIndex socketIndex, const u8* dataBuffer, \
	size_t dataBufferSize, const KplNetAddress& netAddressReceiver, \
	u16 netPortReceiver)
/** 
 * @return (1) the # of elements written to o_dataBuffer  (2) the received data 
 *         into `o_dataBuffer` (3) the network address from which the data was 
 *         sent into `o_netAddressSender`.  If an error occurs, a value < 0 is 
 *         returned.
 */
#define PLATFORM_SOCKET_RECEIVE(name) i32 name(\
	KplSocketIndex socketIndex, u8* o_dataBuffer, \
	size_t dataBufferSize, KplNetAddress* o_netAddressSender, \
	u16* o_netPortSender)
using KplLockHandle = u8;
/**
 * @return a handle == `0` if the request wasn't able to complete
 */
#define PLATFORM_RESERVE_LOCK(name) KplLockHandle name()
#define PLATFORM_LOCK(name) void name(KplLockHandle hLock)
#define PLATFORM_UNLOCK(name) void name(KplLockHandle hLock)
#define PLATFORM_MOUSE_SET_HIDDEN(name) void name(bool value)
/**
 * When the mouse is set to "relative mode", the cursor disappears and the 
 * platform stops reporting the GameMouse's windowPosition (the value freezes to 
 * the last value it was before relative mode was activated).  The platform will 
 * continue to report relative mouse axes during this time, including the wheel 
 * delta position.  When relative mode is turned off, the platform returns the 
 * mouse cursor to the last known screen-space position before relative mode was 
 * activated.
 */
#define PLATFORM_MOUSE_SET_RELATIVE_MODE(name) void name(bool value)
using KplWindowHandle = void*;
struct KplWindowMetaData
{
	i8 cStrTitle[128];
	KplWindowHandle handle;
};
/**
 * @return The # of KplWindowMetaData elements obtained from the platform.  
 *         If an error occurs, a number < 0 is returned.
 */
#define PLATFORM_ENUMERATE_WINDOWS(name) i32 name(\
	KplWindowMetaData* o_metaArray, u32 metaArrayCapacity)
/**
 * If an error occurs, *hWindow will be automatically nullified.
 */
#define PLATFORM_GET_WINDOW_RAW_IMAGE(name) RawImage name(\
	KplWindowHandle* hWindow, \
	fnSig_korlCallbackRequestMemory* callbackRequestMemory, \
	void* requestMemoryUserData)
typedef PLATFORM_POST_JOB(fnSig_platformPostJob);
typedef PLATFORM_JOB_VALID(fnSig_platformJobValid);
typedef PLATFORM_JOB_DONE(fnSig_platformJobDone);
typedef PLATFORM_IMGUI_ALLOC(fnSig_platformImguiAlloc);
typedef PLATFORM_IMGUI_FREE(fnSig_platformImguiFree);
typedef PLATFORM_DECODE_PNG(fnSig_platformDecodePng);
typedef PLATFORM_DECODE_AUDIO_FILE(fnSig_platformDecodeAudioFile);
typedef PLATFORM_IS_FULLSCREEN(fnSig_platformIsFullscreen);
typedef PLATFORM_SET_FULLSCREEN(fnSig_platformSetFullscreen);
typedef PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(
	fnSig_platformGetGamePadActiveButton);
typedef PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(fnSig_platformGetGamePadActiveAxis);
typedef PLATFORM_GET_GAME_PAD_PRODUCT_NAME(fnSig_platformGetGamePadProductName);
typedef PLATFORM_GET_GAME_PAD_PRODUCT_GUID(fnSig_platformGetGamePadProductGuid);
typedef PLATFORM_GET_FILE_HANDLE(fnSig_platformGetFileHandle);
typedef PLATFORM_RELEASE_FILE_HANDLE(fnSig_platformReleaseFileHandle);
typedef PLATFORM_GET_FILE_WRITE_TIME(fnSig_platformGetFileWriteTime);
typedef PLATFORM_IS_FILE_CHANGED(fnSig_platformIsFileChanged);
typedef PLATFORM_GET_FILE_BYTE_SIZE(fnSig_platformGetFileByteSize);
typedef PLATFORM_READ_ENTIRE_FILE(fnSig_platformReadEntireFile);
typedef PLATFORM_WRITE_ENTIRE_FILE(fnSig_platformWriteEntireFile);
typedef PLATFORM_CREATE_DIRECTORY(fnSig_platformCreateDirectory);
typedef PLATFORM_GET_DIRECTORY_ENTRIES(fnSig_platformGetDirectoryEntries);
typedef PLATFORM_DESTROY_DIRECTORY_ENTRY(fnSig_platformDestroyDirectoryEntry);
typedef PLATFORM_RENAME_DIRECTORY_ENTRY(fnSig_platformRenameDirectoryEntry);
typedef PLATFORM_GET_TIMESTAMP(fnSig_platformGetTimeStamp);
typedef PLATFORM_SLEEP_FROM_TIMESTAMP(fnSig_platformSleepFromTimestamp);
typedef PLATFORM_SECONDS_SINCE_TIMESTAMP(fnSig_platformSecondsSinceTimestamp);
typedef PLATFORM_SECONDS_BETWEEN_TIMESTAMPS(
	fnSig_platformSecondsBetweenTimestamps);
typedef PLATFORM_MICROSECONDS_BETWEEN_TIMESTAMPS(
	fnSig_platformMicroSecondsBetweenTimestamps);
typedef PLATFORM_GET_DATESTAMP(fnSig_platformGetDateStamp);
typedef PLATFORM_GENERATE_DATESTAMP_STRING(
	fnSig_platformGenerateDateStampString);
typedef PLATFORM_NET_RESOLVE_ADDRESS(fnSig_platformNetResolveAddress);
typedef PLATFORM_SOCKET_OPEN_UDP(fnSig_platformSocketOpenUdp);
typedef PLATFORM_SOCKET_CLOSE(fnSig_platformSocketClose);
typedef PLATFORM_SOCKET_SEND(fnSig_platformSocketSend);
typedef PLATFORM_SOCKET_RECEIVE(fnSig_platformSocketReceive);
typedef PLATFORM_RESERVE_LOCK(fnSig_platformReserveLock);
typedef PLATFORM_LOCK(fnSig_platformLock);
typedef PLATFORM_UNLOCK(fnSig_platformUnlock);
typedef PLATFORM_MOUSE_SET_HIDDEN(fnSig_platformMouseSetHidden);
typedef PLATFORM_MOUSE_SET_RELATIVE_MODE(fnSig_platformMouseSetRelativeMode);
typedef PLATFORM_ENUMERATE_WINDOWS(fnSig_platformEnumerateWindows);
typedef PLATFORM_GET_WINDOW_RAW_IMAGE(fnSig_platformGetWindowRawImage);
struct KorlPlatformApi
{
	fnSig_platformPostJob* postJob;
	fnSig_platformJobValid* jobValid;
	fnSig_platformJobDone* jobDone;
	fnSig_platformLog* log;
	fnSig_korlPlatformAssertFailure* assertFailure;
	fnSig_platformDecodePng* decodePng;
	fnSig_platformDecodeAudioFile* decodeAudioFile;
	fnSig_platformGetFileHandle* getFileHandle;
	fnSig_platformReleaseFileHandle* releaseFileHandle;
	fnSig_platformGetFileWriteTime* getFileWriteTime;
	fnSig_platformIsFileChanged* isFileChanged;
	fnSig_platformGetFileByteSize* getFileByteSize;
	fnSig_platformReadEntireFile* readEntireFile;
	fnSig_platformWriteEntireFile* writeEntireFile;
	fnSig_platformCreateDirectory* createDirectory;
	fnSig_platformGetDirectoryEntries* getDirectoryEntries;
	fnSig_platformDestroyDirectoryEntry* destroyDirectoryEntry;
	fnSig_platformRenameDirectoryEntry* renameDirectoryEntry;
	fnSig_platformIsFullscreen* isFullscreen;
	fnSig_platformSetFullscreen* setFullscreen;
	fnSig_platformGetGamePadActiveButton* getGamePadActiveButton;
	fnSig_platformGetGamePadActiveAxis* getGamePadActiveAxis;
	fnSig_platformGetGamePadProductName* getGamePadProductName;
	fnSig_platformGetGamePadProductGuid* getGamePadProductGuid;
	fnSig_platformGetTimeStamp* getTimeStamp;
	fnSig_platformSleepFromTimestamp* sleepFromTimeStamp;
	fnSig_platformSecondsSinceTimestamp* secondsSinceTimeStamp;
	fnSig_platformSecondsBetweenTimestamps* secondsBetweenTimeStamps;
	fnSig_platformMicroSecondsBetweenTimestamps* microSecondsBetweenTimeStamps;
	fnSig_platformGetDateStamp* getDateStamp;
	fnSig_platformGenerateDateStampString* generateDateStampString;
	fnSig_platformNetResolveAddress* netResolveAddress;
	fnSig_platformSocketOpenUdp* socketOpenUdp;
	fnSig_platformSocketClose* socketClose;
	fnSig_platformSocketSend* socketSend;
	fnSig_platformSocketReceive* socketReceive;
	fnSig_platformReserveLock* reserveLock;
	fnSig_platformLock* lock;
	fnSig_platformUnlock* unlock;
	fnSig_platformMouseSetHidden* mouseSetHidden;
	fnSig_platformMouseSetRelativeMode* mouseSetRelativeMode;
	fnSig_platformEnumerateWindows* enumerateWindows;
	fnSig_platformGetWindowRawImage* getWindowRawImage;
};
/***************************************************** END PLATFORM INTERFACE */
struct GameMemory
{
	void* permanentMemory;
	u64   permanentMemoryBytes;
	void* transientMemory;
	u64   transientMemoryBytes;
	KorlPlatformApi kpl;
	KrbApi krb;
	void* imguiContext;
	fnSig_platformImguiAlloc* platformImguiAlloc;
	fnSig_platformImguiFree* platformImguiFree;
	void* imguiAllocUserData;
};
struct GameAudioBuffer
{
	// A sample block is a contiguous collection of all samples from all 
	//	channels for a single moment in time.
	SoundSample* sampleBlocks;
	u32 lockedSampleBlockCount;
	u32 soundSampleHz;
	u8 numSoundChannels;
	u8 numSoundChannels_PADDING[7];
};
enum class ButtonState : u8
	{ NOT_PRESSED
	, PRESSED
	, HELD
};
#define KORL_BUTTON_ON(x) ((x) > ButtonState::NOT_PRESSED)
#define KORL_BUTTON_OFF(x) ((x) == ButtonState::NOT_PRESSED)
#define KORL_BUTTON_NEW_PRESS(x) ((x) == ButtonState::PRESSED)
struct GameMouse
{
	/** 
	 * - relative to the upper-left corner of the window
	 * - signed because it should be possible for this value to lie outside of 
	 *   the window, so this should be checked for!  the cursor is inside the 
	 *   window IFF it falls in the region: 
	 *       v2i32{ [0, windowDimensions.x), [0, windowDimensions.y) }
	 */
	v2i32 windowPosition;
	v2i32 deltaPosition;
	i32 deltaWheel;
	union
	{
		ButtonState vKeys[5];
		struct 
		{
			ButtonState left;
			ButtonState right;
			ButtonState middle;
			ButtonState forward;
			ButtonState back;
#if INTERNAL_BUILD
			/** KEEP THIS THE LAST VAR IN THE STRUCT ! ! ! 
			 * This is used to make sure the size of the vKeys array matches the
			 * # of buttons in this anonymous struct.
			*/
			ButtonState DUMMY_LAST_BUTTON_STATE;
#endif
		};
	};
};
struct GameKeyboard
{
	struct Modifiers
	{
		bool alt;
		bool shift;
		bool control;
	} modifiers;
	union
	{
		ButtonState vKeys[95];
		struct 
		{
			ButtonState a;
			ButtonState b;
			ButtonState c;
			ButtonState d;
			ButtonState e;
			ButtonState f;
			ButtonState g;
			ButtonState h;
			ButtonState i;
			ButtonState j;
			ButtonState k;
			ButtonState l;
			ButtonState m;
			ButtonState n;
			ButtonState o;
			ButtonState p;
			ButtonState q;
			ButtonState r;
			ButtonState s;
			ButtonState t;
			ButtonState u;
			ButtonState v;
			ButtonState w;
			ButtonState x;
			ButtonState y;
			ButtonState z;
			ButtonState comma;
			ButtonState period;
			ButtonState slashForward;
			ButtonState slashBack;
			ButtonState curlyBraceLeft;
			ButtonState curlyBraceRight;
			ButtonState semicolon;
			ButtonState quote;
			ButtonState grave;
			ButtonState tenkeyless_0;
			ButtonState tenkeyless_1;
			ButtonState tenkeyless_2;
			ButtonState tenkeyless_3;
			ButtonState tenkeyless_4;
			ButtonState tenkeyless_5;
			ButtonState tenkeyless_6;
			ButtonState tenkeyless_7;
			ButtonState tenkeyless_8;
			ButtonState tenkeyless_9;
			ButtonState tenkeyless_minus;
			ButtonState equals;
			ButtonState backspace;
			ButtonState escape;
			ButtonState enter;
			ButtonState space;
			ButtonState tab;
			ButtonState shiftLeft;
			ButtonState shiftRight;
			ButtonState controlLeft;
			ButtonState controlRight;
			ButtonState altLeft;
			ButtonState altRight;
			ButtonState f1;
			ButtonState f2;
			ButtonState f3;
			ButtonState f4;
			ButtonState f5;
			ButtonState f6;
			ButtonState f7;
			ButtonState f8;
			ButtonState f9;
			ButtonState f10;
			ButtonState f11;
			ButtonState f12;
			ButtonState arrowUp;
			ButtonState arrowDown;
			ButtonState arrowLeft;
			ButtonState arrowRight;
			ButtonState insert;
			ButtonState deleteKey;
			ButtonState home;
			ButtonState end;
			ButtonState pageUp;
			ButtonState pageDown;
			ButtonState numpad_0;
			ButtonState numpad_1;
			ButtonState numpad_2;
			ButtonState numpad_3;
			ButtonState numpad_4;
			ButtonState numpad_5;
			ButtonState numpad_6;
			ButtonState numpad_7;
			ButtonState numpad_8;
			ButtonState numpad_9;
			ButtonState numpad_period;
			ButtonState numpad_divide;
			ButtonState numpad_multiply;
			ButtonState numpad_minus;
			ButtonState numpad_add;
#if INTERNAL_BUILD
			/** KEEP THIS THE LAST VAR IN THE STRUCT ! ! ! 
			 * This is used to make sure the size of the vKeys array matches the
			 * # of buttons in this anonymous struct.
			*/
			ButtonState DUMMY_LAST_BUTTON_STATE;
#endif
		};
	};
};
/** the GamePadType serves two purposes: 
 * - determining whether or not the controller is plugged in
 * - determining what type of controller it is 
 */
enum class GamePadType : u8
	{ UNPLUGGED
	, XINPUT
	, DINPUT_GENERIC
};
struct GamePad
{
	GamePadType type;
	union
	{
		/* all axes have a value in the range [-1,1] */
		f32 axes[6];
		struct
		{
			v2f32 stickLeft;
			v2f32 stickRight;
			f32 triggerLeft;
			f32 triggerRight;
	#if INTERNAL_BUILD
			/** KEEP THIS THE LAST VAR IN THE STRUCT ! ! ! 
			 * See similar struct in GameKeyboard for additional info.
			*/
			f32 DUMMY_LAST_AXIS;
	#endif // INTERNAL_BUILD
		};
	};
	union
	{
		ButtonState buttons[16];
		struct
		{
			ButtonState faceUp;
			ButtonState faceDown;
			ButtonState faceLeft;
			ButtonState faceRight;
			ButtonState start;
			ButtonState back;
			ButtonState dPadUp;
			ButtonState dPadDown;
			ButtonState dPadLeft;
			ButtonState dPadRight;
			ButtonState shoulderLeft;
			ButtonState shoulderRight;
			ButtonState shoulderLeft2;
			ButtonState shoulderRight2;
			ButtonState stickClickLeft;
			ButtonState stickClickRight;
	#if INTERNAL_BUILD
			/** KEEP THIS THE LAST VAR IN THE STRUCT ! ! ! 
			 * See similar struct in GameKeyboard for additional info.
			*/
			ButtonState DUMMY_LAST_BUTTON_STATE;
	#endif // INTERNAL_BUILD
		};
	#if INTERNAL_BUILD
		i8 buttons_PADDING[1];
	#else
		i8 buttons_PADDING[2];
	#endif // INTERNAL_BUILD
	};
	float normalizedMotorSpeedLeft;
	float normalizedMotorSpeedRight;
};
/* GAME INTERFACE *************************************************************/
/**
 * Guaranteed to be called once when the application starts.
 */
#define GAME_INITIALIZE(name) void name(GameMemory& memory)
/**
 * OPTIONAL.  Guaranteed to be called every time the game code is reloaded.
 * Guaranteed to be called BEFORE GAME_INITIALIZE.
 */
#define GAME_ON_RELOAD_CODE(name) \
	void name(GameMemory& memory, bool gameMemoryIsInitialized)
#define GAME_RENDER_AUDIO(name) \
	void name(GameAudioBuffer& audioBuffer, u32 sampleBlocksConsumed)
/** 
 * @return false if the platform should close the game application
 */
#define GAME_UPDATE_AND_DRAW(name) \
	bool name(f32 deltaSeconds, v2u32 windowDimensions, \
	          const GameMouse& gameMouse, const GameKeyboard& gameKeyboard, \
	          GamePad* gamePadArray, u8 numGamePads, bool windowIsFocused)
/** 
 * OPTIONAL.  Called IMMEDIATELY before the game code is unloaded from memory.
*/
#define GAME_ON_PRE_UNLOAD(name) void name()
typedef GAME_INITIALIZE(fnSig_gameInitialize);
typedef GAME_ON_RELOAD_CODE(fnSig_gameOnReloadCode);
typedef GAME_RENDER_AUDIO(fnSig_gameRenderAudio);
typedef GAME_UPDATE_AND_DRAW(fnSig_gameUpdateAndDraw);
typedef GAME_ON_PRE_UNLOAD(fnSig_gameOnPreUnload);
/********************************************************* END GAME INTERFACE */
