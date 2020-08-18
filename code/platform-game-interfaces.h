#pragma once
#include "kutil.h"
#include "krb-interface.h"
#include "kGeneralAllocator.h"
#include "kmath.h"
// Data structures which must be the same for the Platform & Game layers ///////
/* PLATFORM INTERFACE *********************************************************/
enum class PlatformLogCategory : u8
{
	K_INFO,
	K_WARNING,
	K_ERROR
};
struct RawImage
{
	u32 sizeX;
	u32 sizeY;
	// pixel data layout: 0xRrGgBbAa
	u8* pixelData;
};
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
/* 
FlipbookMetaData asset files are just text.  They must contain ONE of EACH 
	property of the struct.  Each property must be contained on a new line.
	The property identifier (left side) MUST match exactly to the example below.  
	The identifier & value (right side) are separated by a ':' character.  Any 
	whitespace aside from '\n' characters are allowed at any time.  The file 
	extention MUST be ".fbm".
A `frame-count` of 0 means the texture asset is "fully saturated" with flipbook 
	pages.  This means the true frame count is equivilant to: 
	`flipbookPageRows*flipbookPageCols` where 
		`flipbookPageRows` == textureAsset.sizeY / `frame-size-y` and
		`flipbookPageCols` == textureAsset.sizeX / `frame-size-x`
A `frame-size-x/y` of {0,0} means the entire texture asset is a single frame.
`default-anchor-ratio-x/y` is relative to the upper-left corner of the texture 
	asset.
----- Example of a flipbook meta data asset text file: -----
frame-size-x              : 48
frame-size-y              : 48
frame-count               : 6
texture-asset-file-name   : fighter-exhaust.png
default-repeat            : 1
default-reverse           : 0
default-seconds-per-frame : 0.05
default-anchor-ratio-x    : 0.5
default-anchor-ratio-y    : 0.5
*/
struct FlipbookMetaData
{
	u32 frameSizeX;
	u32 frameSizeY;
	u16 frameCount;
	bool defaultRepeat;
	bool defaultReverse;
	f32 defaultSecondsPerFrame;
	size_t textureKAssetIndex;
	f32 defaultAnchorRatioX;
	f32 defaultAnchorRatioY;
};
using FileWriteTime = u64;
/** Do not use this value directly, since the meaning of this data is 
 * platform-dependent.  Instead, use the platform APIs which uses this type. */
using PlatformTimeStamp = u64;
using JobQueueTicket = u32;
#define JOB_QUEUE_FUNCTION(name) \
	void name(void* data, u32 threadId)
typedef JOB_QUEUE_FUNCTION(fnSig_jobQueueFunction);
/** 
 * @return the job ticket which uses `function`.  If `function` is nullptr, the 
 *         return value is guaranteed to be an INVALID ticket.
*/
#define PLATFORM_POST_JOB(name) \
	JobQueueTicket name(fnSig_jobQueueFunction* function, void* data)
#define PLATFORM_JOB_VALID(name) \
	bool name(JobQueueTicket* ticket)
#define PLATFORM_JOB_DONE(name) \
	bool name(JobQueueTicket* ticket)
#define PLATFORM_LOG(name) \
	void name(const char* sourceFileName, u32 sourceFileLineNumber, \
	          PlatformLogCategory logCategory, const char* formattedString, ...)
#define PLATFORM_ASSERT(name) \
	void name(bool expression)
#define PLATFORM_IMGUI_ALLOC(name) \
	void* name(size_t sz, void* user_data)
#define PLATFORM_IMGUI_FREE(name) \
	void  name(void* ptr, void* user_data)
#define PLATFORM_DECODE_Z85_PNG(name) \
	RawImage name(const u8* z85PngData, size_t z85PngNumBytes, \
	              KgaHandle pixelDataAllocator)
#define PLATFORM_DECODE_Z85_WAV(name) \
	RawSound name(const u8* z85WavData, size_t z85WavNumBytes, \
	              KgaHandle sampleDataAllocator)
/**
 * @return If there is a failure loading the file, an invalid RawSound 
 *         containing sampleData==nullptr is returned.
 */
#define PLATFORM_LOAD_WAV(name) \
	RawSound name(const char* fileName, KgaHandle sampleDataAllocator)
/**
 * @return If there is a failure loading the file, an invalid RawSound 
 *         containing sampleData==nullptr is returned.
 */
#define PLATFORM_LOAD_OGG(name) \
	RawSound name(const char* fileName, KgaHandle sampleDataAllocator)
/**
 * @return If there is a failure loading the file, an invalid RawImage 
 *         containing pixelData==nullptr is returned.
 */
#define PLATFORM_LOAD_PNG(name) \
	RawImage name(const char* fileName, KgaHandle pixelDataAllocator)
#define PLATFORM_LOAD_FLIPBOOK_META(name) \
	bool name(const char* fileName, FlipbookMetaData* o_fbMeta, \
	          char* o_texAssetFileName, size_t texAssetFileNameBufferSize)
#define PLATFORM_GET_ASSET_WRITE_TIME(name) \
	FileWriteTime name(const char* assetFileName)
#define PLATFORM_IS_ASSET_CHANGED(name) \
	bool name(const char* assetFileName, FileWriteTime lastWriteTime)
#define PLATFORM_IS_ASSET_AVAILABLE(name) \
	bool name(const char* assetFileName)
#define PLATFORM_IS_FULLSCREEN(name) \
	bool name()
global_variable const u16 INVALID_PLATFORM_BUTTON_INDEX = u16(~0);
#define PLATFORM_SET_FULLSCREEN(name) \
	void name(bool isFullscreenDesired)
/** Don't use this API for anything related to game development!!!  The only 
 * reason this exists is to discover hardware-specific controller input maps. 
 * @return INVALID_PLATFORM_BUTTON_INDEX if there is no active button OR if 
 *         there are more than one buttons active!
 */
#define PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(name) \
	u16 name(u8 gamePadIndex)
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
#define PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(name) \
	PlatformGamePadActiveAxis name(u8 gamePadIndex)
/** Don't use this API for anything related to game development!!!  The only 
 * reason this exists is to discover hardware-specific controller input maps. 
 */
#define PLATFORM_GET_GAME_PAD_PRODUCT_NAME(name) \
	void name(u8 gamePadIndex, char* o_buffer, size_t bufferSize)
/** Don't use this API for anything related to game development!!!  The only 
 * reason this exists is to discover hardware-specific controller input maps. 
 */
#define PLATFORM_GET_GAME_PAD_PRODUCT_GUID(name) \
	void name(u8 gamePadIndex, char* o_buffer, size_t bufferSize)
#define PLATFORM_GET_TIMESTAMP(name) \
	PlatformTimeStamp name()
#define PLATFORM_SLEEP_FROM_TIMESTAMP(name) \
	void name(PlatformTimeStamp pts, f32 desiredDeltaSeconds)
#define PLATFORM_SECONDS_SINCE_TIMESTAMP(name) \
	f32 name(PlatformTimeStamp pts)
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
#define PLATFORM_NET_RESOLVE_ADDRESS(name) \
	KplNetAddress name(const char* ansiAddress)
/** 
 * @param listenPort If this is 0, the port which the socket listens on is 
 *                   chosen by the platform automatically.  This value does not 
 *                   prevent the user application from SENDING to other ports.
 * @param address if this is nullptr, the socket is bound to ANY address
 * @return KPL_INVALID_SOCKET_INDEX if the socket could not be created for any 
 *         reason
*/
#define PLATFORM_SOCKET_OPEN_UDP(name) KplSocketIndex name(u16 listenPort)
#define PLATFORM_SOCKET_CLOSE(name) void name(KplSocketIndex socketIndex)
/**
 * @return the # of bytes sent.  If an error occurs, a value < 0 is returned.  
 *         If the socket or underlying winsock system is not ready to send right 
 *         now, 0 is returned (no errors occurred)
 */
#define PLATFORM_SOCKET_SEND(name) \
	i32 name(KplSocketIndex socketIndex, const u8* dataBuffer, \
	         size_t dataBufferSize, const KplNetAddress& netAddressReceiver, \
	         u16 netPortReceiver)
/** 
 * @return (1) the # of elements written to o_dataBuffer  (2) the received data 
 *         into `o_dataBuffer` (3) the network address from which the data was 
 *         sent into `o_netAddressSender`.  If an error occurs, a value < 0 is 
 *         returned.
 */
#define PLATFORM_SOCKET_RECEIVE(name) \
	i32 name(KplSocketIndex socketIndex, u8* o_dataBuffer, \
	         size_t dataBufferSize, KplNetAddress* o_netAddressSender, \
	         u16* o_netPortSender)
typedef PLATFORM_POST_JOB(fnSig_platformPostJob);
typedef PLATFORM_JOB_VALID(fnSig_platformJobValid);
typedef PLATFORM_JOB_DONE(fnSig_platformJobDone);
typedef PLATFORM_LOG(fnSig_platformLog);
typedef PLATFORM_ASSERT(fnSig_platformAssert);
typedef PLATFORM_IMGUI_ALLOC(fnSig_platformImguiAlloc);
typedef PLATFORM_IMGUI_FREE(fnSig_platformImguiFree);
typedef PLATFORM_DECODE_Z85_PNG(fnSig_platformDecodeZ85Png);
typedef PLATFORM_DECODE_Z85_WAV(fnSig_platformDecodeZ85Wav);
typedef PLATFORM_LOAD_WAV(fnSig_platformLoadWav);
typedef PLATFORM_LOAD_OGG(fnSig_platformLoadOgg);
typedef PLATFORM_LOAD_PNG(fnSig_platformLoadPng);
typedef PLATFORM_LOAD_FLIPBOOK_META(fnSig_platformLoadFlipbookMeta);
typedef PLATFORM_GET_ASSET_WRITE_TIME(fnSig_platformGetAssetWriteTime);
typedef PLATFORM_IS_ASSET_CHANGED(fnSig_platformIsAssetChanged);
typedef PLATFORM_IS_ASSET_AVAILABLE(fnSig_platformIsAssetAvailable);
typedef PLATFORM_IS_FULLSCREEN(fnSig_platformIsFullscreen);
typedef PLATFORM_SET_FULLSCREEN(fnSig_platformSetFullscreen);
typedef PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(
	                                      fnSig_platformGetGamePadActiveButton);
typedef PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(fnSig_platformGetGamePadActiveAxis);
typedef PLATFORM_GET_GAME_PAD_PRODUCT_NAME(fnSig_platformGetGamePadProductName);
typedef PLATFORM_GET_GAME_PAD_PRODUCT_GUID(fnSig_platformGetGamePadProductGuid);
struct PlatformDebugReadFileResult
{
	u32 dataBytes;
	u32 dataBytes_PADDING;
	void* data;
};
/** @return a valid result (non-zero data & dataBytes) if successful */
#define PLATFORM_READ_ENTIRE_FILE(name) \
	PlatformDebugReadFileResult name(char* fileName)
#define PLATFORM_FREE_FILE_MEMORY(name) \
	void name(void* fileMemory)
#define PLATFORM_WRITE_ENTIRE_FILE(name) \
	bool name(char* fileName, void* data, u32 dataBytes)
typedef PLATFORM_READ_ENTIRE_FILE(fnSig_PlatformReadEntireFile);
typedef PLATFORM_FREE_FILE_MEMORY(fnSig_PlatformFreeFileMemory);
typedef PLATFORM_WRITE_ENTIRE_FILE(fnSig_PlatformWriteEntireFile);
typedef PLATFORM_GET_TIMESTAMP(fnSig_PlatformGetTimeStamp);
typedef PLATFORM_SLEEP_FROM_TIMESTAMP(fnSig_PlatformSleepFromTimestamp);
typedef PLATFORM_SECONDS_SINCE_TIMESTAMP(fnSig_PlatformSecondsSinceTimestamp);
typedef PLATFORM_NET_RESOLVE_ADDRESS(fnSig_PlatformNetResolveAddress);
typedef PLATFORM_SOCKET_OPEN_UDP(fnSig_PlatformSocketOpenUdp);
typedef PLATFORM_SOCKET_CLOSE(fnSig_PlatformSocketClose);
typedef PLATFORM_SOCKET_SEND(fnSig_PlatformSocketSend);
typedef PLATFORM_SOCKET_RECEIVE(fnSig_PlatformSocketReceive);
struct KmlPlatformApi
{
	fnSig_platformPostJob* postJob;
	fnSig_platformJobValid* jobValid;
	fnSig_platformJobDone* jobDone;
	fnSig_platformLog* log;
	fnSig_platformAssert* assert;
	fnSig_platformDecodeZ85Png* decodeZ85Png;
	fnSig_platformDecodeZ85Wav* decodeZ85Wav;
	fnSig_platformLoadWav* loadWav;
	fnSig_platformLoadOgg* loadOgg;
	fnSig_platformLoadPng* loadPng;
	fnSig_platformLoadFlipbookMeta* loadFlipbookMeta;
	fnSig_platformGetAssetWriteTime* getAssetWriteTime;
	fnSig_platformIsAssetChanged* isAssetChanged;
	fnSig_platformIsAssetAvailable* isAssetAvailable;
	fnSig_platformIsFullscreen* isFullscreen;
	fnSig_platformSetFullscreen* setFullscreen;
	fnSig_platformGetGamePadActiveButton* getGamePadActiveButton;
	fnSig_platformGetGamePadActiveAxis* getGamePadActiveAxis;
	fnSig_platformGetGamePadProductName* getGamePadProductName;
	fnSig_platformGetGamePadProductGuid* getGamePadProductGuid;
	fnSig_PlatformGetTimeStamp* getTimeStamp;
	fnSig_PlatformSleepFromTimestamp* sleepFromTimeStamp;
	fnSig_PlatformSecondsSinceTimestamp* secondsSinceTimeStamp;
	fnSig_PlatformNetResolveAddress* netResolveAddress;
	fnSig_PlatformSocketOpenUdp* socketOpenUdp;
	fnSig_PlatformSocketClose* socketClose;
	fnSig_PlatformSocketSend* socketSend;
	fnSig_PlatformSocketReceive* socketReceive;
#if INTERNAL_BUILD
	fnSig_PlatformReadEntireFile* readEntireFile;
	fnSig_PlatformFreeFileMemory* freeFileMemory;
	fnSig_PlatformWriteEntireFile* writeEntireFile;
#endif// INTERNAL_BUILD
};
/***************************************************** END PLATFORM INTERFACE */
struct GameMemory
{
	void* permanentMemory;
	u64   permanentMemoryBytes;
	void* transientMemory;
	u64   transientMemoryBytes;
	KmlPlatformApi kpl;
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
	          GameKeyboard& gameKeyboard, GamePad* gamePadArray, \
	          u8 numGamePads, bool windowIsFocused)
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