#include "win32-main.h"
#include "win32-dsound.h"
#include "win32-xinput.h"
#include "win32-directinput.h"
#include "win32-krb-opengl.h"
#include "win32-jobQueue.h"
#include "kGeneralAllocator.h"
#include <cstdio>
#include <ctime>
#include <dbghelp.h>
#include <strsafe.h>
#include <ShlObj.h>
#include <Dbt.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
                             HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include "imgui/imgui_impl_opengl2.h"
#include "z85.h"
#include "stb/stb_image.h"
#include "stb/stb_vorbis.h"
// Allow the pre-processor to store compiler definitions as string literals //
//	Source: https://stackoverflow.com/a/39108392
#define _DEFINE_TO_CSTR(define) #define
#define DEFINE_TO_CSTR(d) _DEFINE_TO_CSTR(d)
global_variable const TCHAR APPLICATION_NAME[] = 
                                    TEXT(DEFINE_TO_CSTR(KML_APP_NAME));
global_variable const TCHAR APPLICATION_VERSION[] = 
                                    TEXT(DEFINE_TO_CSTR(KML_APP_VERSION));
global_variable const TCHAR FILE_NAME_GAME_DLL[] = 
                                    TEXT(DEFINE_TO_CSTR(KML_GAME_DLL_FILENAME));
global_variable const f32 MAX_GAME_DELTA_SECONDS = 1.f / KML_MINIMUM_FRAME_RATE;
global_variable bool g_running;
global_variable bool g_displayCursor;
global_variable bool g_isFocused;
/* this variable exists because mysterious COM incantations require us to check 
	whether or not a DirectInput device is Xinput compatible OUTSIDE of the 
	windows message loop */
global_variable bool g_deviceChangeDetected;
global_variable HCURSOR g_cursorArrow;
global_variable HCURSOR g_cursorSizeVertical;
global_variable HCURSOR g_cursorSizeHorizontal;
global_variable HCURSOR g_cursorSizeNeSw;
global_variable HCURSOR g_cursorSizeNwSe;
global_variable W32OffscreenBuffer g_backBuffer;
global_variable LARGE_INTEGER g_perfCounterHz;
global_variable KgaHandle g_genAllocStbImage;
global_variable KgaHandle g_genAllocImgui;
global_variable stb_vorbis_alloc g_oggVorbisAlloc;
global_variable CRITICAL_SECTION g_assetAllocationCsLock;
global_variable CRITICAL_SECTION g_vorbisAllocationCsLock;
global_variable CRITICAL_SECTION g_stbiAllocationCsLock;
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
global_variable CRITICAL_SECTION g_logCsLock;
global_variable bool g_hasReceivedException;
// @assumption: once we have written a crash dump, there is never a need to 
//	write any more, let alone continue execution.
global_variable bool g_hasWrittenCrashDump;
///TODO: handle file paths longer than MAX_PATH in the future...
global_variable TCHAR g_pathToExe[MAX_PATH];
global_variable TCHAR g_pathToAssets[MAX_PATH];
global_variable TCHAR g_pathTemp[MAX_PATH];
global_variable TCHAR g_pathLocalAppData[MAX_PATH];
global_variable JobQueue g_jobQueue;
global_variable WINDOWPLACEMENT g_lastKnownWindowedPlacement;
global_variable GamePad g_gamePadArrayA[XUSER_MAX_COUNT + 
                                            CARRAY_COUNT(g_dInputDevices)] = {};
global_variable GamePad g_gamePadArrayB[XUSER_MAX_COUNT + 
                                            CARRAY_COUNT(g_dInputDevices)] = {};
global_variable GamePad* g_gamePadArrayCurrentFrame  = g_gamePadArrayA;
global_variable GamePad* g_gamePadArrayPreviousFrame = g_gamePadArrayB;
struct W32ThreadInfo
{
	u32 index;
	JobQueue* jobQueue;
};
///TODO: rename all these windows implementations of these platform functions 
///      appropriately using w32 prefix like everything else!
internal PLATFORM_IS_FULLSCREEN(platformIsFullscreen)
{
	const DWORD windowStyle = GetWindowLong(g_mainWindow, GWL_STYLE);
	if(windowStyle == 0)
	{
		KLOG(ERROR, "Failed to get window style!");
		return false;
	}
	return !(windowStyle & WS_OVERLAPPEDWINDOW);
}
internal PLATFORM_SET_FULLSCREEN(platformSetFullscreen)
{
	const DWORD windowStyle = GetWindowLong(g_mainWindow, GWL_STYLE);
	if(windowStyle == 0)
	{
		KLOG(ERROR, "Failed to get window style!");
		return;
	}
	if(windowStyle & WS_OVERLAPPEDWINDOW)
	{
		if(!isFullscreenDesired)
		{
			KLOG(WARNING, "Window is already not fullscreen.  "
			              "Ignoring fullscreen change request.");
			return;
		}
		MONITORINFO monitorInfo = { sizeof(monitorInfo) };
		const HMONITOR monitor = 
			MonitorFromWindow(g_mainWindow, MONITOR_DEFAULTTOPRIMARY);
		if( GetWindowPlacement(g_mainWindow, &g_lastKnownWindowedPlacement) &&
			GetMonitorInfo(monitor, &monitorInfo) )
		{
			if(!SetWindowLong(g_mainWindow, GWL_STYLE, 
			                  windowStyle & ~WS_OVERLAPPEDWINDOW))
			{
				KLOG(ERROR, "Failed to set window style! getlasterror=%i", 
				     GetLastError());
				return;
			}
			const int windowSizeX = 
				monitorInfo.rcMonitor.right  - monitorInfo.rcMonitor.left;
			const int windowSizeY = 
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
			if(!SetWindowPos(g_mainWindow, HWND_TOP, 
			                 monitorInfo.rcMonitor.left,
			                 monitorInfo.rcMonitor.top,
			                 windowSizeX, windowSizeY, 
			                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
			{
				KLOG(ERROR, "Failed to set window position! getlasterror=%i", 
				     GetLastError());
			}
		}
		else
		{
			KLOG(ERROR, "Failed to get monitor info!");
		}
	}
	else
	{
		if(isFullscreenDesired)
		{
			KLOG(WARNING, "Window is already fullscreen.  "
			              "Ignoring fullscreen change request.");
			return;
		}
		if(!SetWindowLong(g_mainWindow, GWL_STYLE, 
		                  windowStyle | WS_OVERLAPPEDWINDOW))
		{
			KLOG(ERROR, "Failed to set window style! getlasterror=%i", 
			     GetLastError());
			return;
		}
		if(!SetWindowPlacement(g_mainWindow, &g_lastKnownWindowedPlacement))
		{
			KLOG(ERROR, "Failed to set window placement! getlasterror=%i", 
			     GetLastError());
			return;
		}
		if(!SetWindowPos(g_mainWindow, NULL, 0, 0, 0, 0, 
		                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
		                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED))
		{
			KLOG(ERROR, "Failed to set window position! getlasterror=%i", 
			     GetLastError());
		}
	}
}
internal PLATFORM_POST_JOB(platformPostJob)
{
	return jobQueuePostJob(&g_jobQueue, function, data);
}
internal PLATFORM_JOB_DONE(platformJobDone)
{
	return jobQueueJobIsDone(&g_jobQueue, ticket);
}
internal bool decodeFlipbookMeta(const PlatformDebugReadFileResult& file, 
                                 FlipbookMetaData* o_fbMeta, 
                                 char* o_texAssetFileName, 
                                 size_t texAssetFileNameBufferSize)
{
	*o_fbMeta = {};
	char*const fileCStr = reinterpret_cast<char*>(file.data);
	u16 fbPropertiesFoundBitflags = 0;
	u8 fbPropertiesFoundCount = 0;
	// destructively read the file line by line //
	// source: https://stackoverflow.com/a/17983619
	char* currLine = fileCStr;
	while(currLine)
	{
		char* nextLine = strchr(currLine, '\n');
		if(nextLine) *nextLine = '\0';
		// parse `currLine` for flipbook meta data //
		{
			char* idValueSeparator = strchr(currLine, ':');
			if(!idValueSeparator)
			{
				return false;
			}
			*idValueSeparator = '\0';
			idValueSeparator++;
			if(strstr(currLine, "frame-size-x"))
			{
				fbPropertiesFoundBitflags |= 1<<0;
				fbPropertiesFoundCount++;
				o_fbMeta->frameSizeX = atoi(idValueSeparator);
			}
			else if(strstr(currLine, "frame-size-y"))
			{
				fbPropertiesFoundBitflags |= 1<<1;
				fbPropertiesFoundCount++;
				o_fbMeta->frameSizeY = atoi(idValueSeparator);
			}
			else if(strstr(currLine, "frame-count"))
			{
				fbPropertiesFoundBitflags |= 1<<2;
				fbPropertiesFoundCount++;
				o_fbMeta->frameCount = 
					kmath::safeTruncateU16(atoi(idValueSeparator));
			}
			else if(strstr(currLine, "texture-asset-file-name"))
			{
				fbPropertiesFoundBitflags |= 1<<3;
				fbPropertiesFoundCount++;
				while(*idValueSeparator && isspace(*idValueSeparator))
				{
					idValueSeparator++;
				}
				for(char* spaceSearchChar = idValueSeparator + 1; 
					*spaceSearchChar; spaceSearchChar++)
				{
					if(isspace(*spaceSearchChar))
					{
						*spaceSearchChar = '\0';
						break;
					}
				}
				const size_t valueStrLength = strlen(idValueSeparator);
				if(valueStrLength >= texAssetFileNameBufferSize - 1)
				{
					return false;
				}
				strcpy_s(o_texAssetFileName, texAssetFileNameBufferSize,
				         idValueSeparator);
			}
			else if(strstr(currLine, "default-repeat"))
			{
				fbPropertiesFoundBitflags |= 1<<4;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultRepeat = atoi(idValueSeparator) != 0;
			}
			else if(strstr(currLine, "default-reverse"))
			{
				fbPropertiesFoundBitflags |= 1<<5;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultReverse = atoi(idValueSeparator) != 0;
			}
			else if(strstr(currLine, "default-seconds-per-frame"))
			{
				fbPropertiesFoundBitflags |= 1<<6;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultSecondsPerFrame = 
					static_cast<f32>(atof(idValueSeparator));
			}
			else if(strstr(currLine, "default-anchor-ratio-x"))
			{
				fbPropertiesFoundBitflags |= 1<<7;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultAnchorRatioX = 
					static_cast<f32>(atof(idValueSeparator));
			}
			else if(strstr(currLine, "default-anchor-ratio-y"))
			{
				fbPropertiesFoundBitflags |= 1<<8;
				fbPropertiesFoundCount++;
				o_fbMeta->defaultAnchorRatioY = 
					static_cast<f32>(atof(idValueSeparator));
			}
		}
		if(nextLine) *nextLine = '\n';
		currLine = nextLine ? (nextLine + 1) : nullptr;
	}
	return (fbPropertiesFoundCount == 9) && 
		(fbPropertiesFoundBitflags == 0x1FF);
}
internal RawImage decodePng(const PlatformDebugReadFileResult& file,
                            KgaHandle pixelDataAllocator)
{
	i32 imgW, imgH, imgNumByteChannels;
	u8*const img = 
		stbi_load_from_memory(reinterpret_cast<u8*>(file.data), file.dataBytes, 
		                      &imgW, &imgH, &imgNumByteChannels, 4);
	kassert(img);
	if(!img)
	{
		KLOG(ERROR, "stbi_load_from_memory failure!");
		return {};
	}
	defer(stbi_image_free(img));
	// Copy the output from STBI to a buffer in our pixelDataAllocator //
	EnterCriticalSection(&g_assetAllocationCsLock);
	u8*const pixelData = reinterpret_cast<u8*>(
		kgaAlloc(pixelDataAllocator, imgW*imgH*4));
	LeaveCriticalSection(&g_assetAllocationCsLock);
	if(!pixelData)
	{
		KLOG(ERROR, "Failed to allocate pixelData!");
		return {};
	}
	memcpy(pixelData, img, imgW*imgH*4);
	return RawImage{
		.sizeX            = kmath::safeTruncateU32(imgW),
		.sizeY            = kmath::safeTruncateU32(imgH), 
		.pixelData        = pixelData };
}
internal PLATFORM_LOAD_PNG(platformLoadPng)
{
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, fileName);
	PlatformDebugReadFileResult file = platformReadEntireFile(szFileFullPath);
	if(!file.data)
	{
		KLOG(ERROR, "Failed to read entire file '%s'!", szFileFullPath);
		return {};
	}
	defer(platformFreeFileMemory(file.data));
	return decodePng(file, pixelDataAllocator);
}
internal PLATFORM_LOAD_FLIPBOOK_META(platformLoadFlipbookMeta)
{
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, fileName);
	PlatformDebugReadFileResult file = platformReadEntireFile(szFileFullPath);
	if(!file.data)
	{
		KLOG(ERROR, "Failed to read entire file '%s'!", szFileFullPath);
		return {};
	}
	defer(platformFreeFileMemory(file.data));
	return decodeFlipbookMeta(file, o_fbMeta, o_texAssetFileName, 
	                          texAssetFileNameBufferSize);
}
internal PLATFORM_LOAD_OGG(platformLoadOgg)
{
	// Load the entire OGG file into memory //
	RawSound result = {};
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, fileName);
	PlatformDebugReadFileResult file = platformReadEntireFile(szFileFullPath);
	if(!file.data)
	{
		KLOG(ERROR, "Failed to read entire file '%s'!", szFileFullPath);
		return result;
	}
	defer(platformFreeFileMemory(file.data));
	// Decode the OGG file into a RawSound //
	int vorbisError;
	EnterCriticalSection(&g_vorbisAllocationCsLock);
	stb_vorbis*const vorbis = 
		stb_vorbis_open_memory(reinterpret_cast<u8*>(file.data), file.dataBytes, 
		                       &vorbisError, &g_oggVorbisAlloc);
	if(vorbisError != VORBIS__no_error)
	{
		LeaveCriticalSection(&g_vorbisAllocationCsLock);
		KLOG(ERROR, "stb_vorbis_open_memory error=%i", vorbisError);
		return result;
	}
	const stb_vorbis_info vorbisInfo = stb_vorbis_get_info(vorbis);
	if(vorbisInfo.channels < 1 || vorbisInfo.channels > 255)
	{
		LeaveCriticalSection(&g_vorbisAllocationCsLock);
		KLOG(ERROR, "vorbisInfo.channels==%i invalid/unsupported for '%s'!",
		     vorbisInfo.channels, szFileFullPath);
		return result;
	}
	if(vorbisInfo.sample_rate != 44100)
	{
		LeaveCriticalSection(&g_vorbisAllocationCsLock);
		KLOG(ERROR, "vorbisInfo.sample_rate==%i invalid/unsupported for '%s'!", 
		     vorbisInfo.sample_rate, szFileFullPath);
		return result;
	}
	const i32 vorbisSampleLength = stb_vorbis_stream_length_in_samples(vorbis);
	LeaveCriticalSection(&g_vorbisAllocationCsLock);
	const size_t sampleDataSize = 
		vorbisSampleLength*vorbisInfo.channels*sizeof(SoundSample);
	EnterCriticalSection(&g_assetAllocationCsLock);
	SoundSample*const sampleData = reinterpret_cast<SoundSample*>(
		kgaAlloc(sampleDataAllocator, sampleDataSize) );
	LeaveCriticalSection(&g_assetAllocationCsLock);
	if(!sampleData)
	{
		KLOG(ERROR, "Failed to allocate %i bytes for sample data!", 
		     sampleDataSize);
		return result;
	}
	EnterCriticalSection(&g_vorbisAllocationCsLock);
	const i32 samplesDecoded = 
		stb_vorbis_get_samples_short_interleaved(vorbis, vorbisInfo.channels, 
		                                         sampleData, 
		                                         vorbisSampleLength * 
		                                            vorbisInfo.channels);
	LeaveCriticalSection(&g_vorbisAllocationCsLock);
	if(samplesDecoded != vorbisSampleLength)
	{
		KLOG(ERROR, "Failed to get samples for '%s'!", szFileFullPath);
		EnterCriticalSection(&g_assetAllocationCsLock);
		kgaFree(sampleDataAllocator, sampleData);
		LeaveCriticalSection(&g_assetAllocationCsLock);
		return result;
	}
	// Assemble & return the final result //
	result.bitsPerSample    = sizeof(SoundSample)*8;
	result.channelCount     = static_cast<u8>(vorbisInfo.channels);
	result.sampleHz         = vorbisInfo.sample_rate;
	result.sampleBlockCount = vorbisSampleLength;
	result.sampleData       = sampleData;
	return result;
}
internal RawSound decodeWav(const PlatformDebugReadFileResult& file,
                            KgaHandle sampleDataAllocator,
							const TCHAR* szFileFullPath)
{
	// Now that we have the entire file in memory, we can verify if the file is
	//	valid and extract necessary meta data simultaneously! //
	u8* currByte = reinterpret_cast<u8*>(file.data);
	union U32Chars
	{
		u32 uInt;
		char chars[4];
	} u32Chars;
	u32Chars.chars[0] = 'R';
	u32Chars.chars[1] = 'I';
	u32Chars.chars[2] = 'F';
	u32Chars.chars[3] = 'F';
	if(u32Chars.uInt != *reinterpret_cast<u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.ChunkId==0x%x invalid for '%s'!", 
		     reinterpret_cast<u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	const u32 riffChunkSize = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	u32Chars.chars[0] = 'W';
	u32Chars.chars[1] = 'A';
	u32Chars.chars[2] = 'V';
	u32Chars.chars[3] = 'E';
	if(u32Chars.uInt != *reinterpret_cast<u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.Format==0x%x invalid/unsupported for '%s'!", 
		     *reinterpret_cast<u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	u32Chars.chars[0] = 'f';
	u32Chars.chars[1] = 'm';
	u32Chars.chars[2] = 't';
	u32Chars.chars[3] = ' ';
	if(u32Chars.uInt != *reinterpret_cast<u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.fmt.id==0x%x invalid for '%s'!", 
		     *reinterpret_cast<u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	const u32 riffFmtChunkSize = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	if(riffFmtChunkSize != 16)
	{
		KLOG(ERROR, "RIFF.fmt.chunkSize==%i invalid/unsupported for '%s'!", 
		     riffFmtChunkSize, szFileFullPath);
		return {};
	}
	const u16 riffFmtAudioFormat = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	if(riffFmtAudioFormat != 1)
	{
		KLOG(ERROR, "RIFF.fmt.audioFormat==%i invalid/unsupported for '%s'!",
		     riffFmtAudioFormat, szFileFullPath);
		return {};
	}
	const u16 riffFmtNumChannels = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	if(riffFmtNumChannels > 255 || riffFmtNumChannels == 0)
	{
		KLOG(ERROR, "RIFF.fmt.numChannels==%i invalid/unsupported for '%s'!",
		     riffFmtNumChannels, szFileFullPath);
		return {};
	}
	const u32 riffFmtSampleRate = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	if(riffFmtSampleRate != 44100)
	{
		KLOG(ERROR, "RIFF.fmt.riffFmtSampleRate==%i invalid/unsupported for "
		     "'%s'!", riffFmtSampleRate, szFileFullPath);
		return {};
	}
	const u32 riffFmtByteRate = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	const u16 riffFmtBlockAlign = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	const u16 riffFmtBitsPerSample = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	if(riffFmtBitsPerSample != sizeof(SoundSample)*8)
	{
		KLOG(ERROR, "RIFF.fmt.riffFmtBitsPerSample==%i invalid/unsupported for "
		     "'%s'!", riffFmtBitsPerSample, szFileFullPath);
		return {};
	}
	if(riffFmtByteRate != 
		riffFmtSampleRate * riffFmtNumChannels * riffFmtBitsPerSample/8)
	{
		KLOG(ERROR, "RIFF.fmt.riffFmtByteRate==%i invalid for '%s'!", 
		     riffFmtByteRate, szFileFullPath);
		return {};
	}
	if(riffFmtBlockAlign != riffFmtNumChannels * riffFmtBitsPerSample/8)
	{
		KLOG(ERROR, "RIFF.fmt.riffFmtBlockAlign==%i invalid for '%s'!", 
		     riffFmtBlockAlign, szFileFullPath);
		return {};
	}
	u32Chars.chars[0] = 'd';
	u32Chars.chars[1] = 'a';
	u32Chars.chars[2] = 't';
	u32Chars.chars[3] = 'a';
	if(u32Chars.uInt != *reinterpret_cast<u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.data.id==0x%x invalid for '%s'!", 
		     *reinterpret_cast<u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	const u32 riffDataSize = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	// Now we can extract the actual sound data from the WAV file //
	if(currByte + riffDataSize > 
		reinterpret_cast<u8*>(file.data) + file.dataBytes)
	{
		KLOG(ERROR, "RIFF.data.size==%i invalid for '%s'!", 
		     riffDataSize, szFileFullPath);
		return {};
	}
	EnterCriticalSection(&g_assetAllocationCsLock);
	SoundSample*const sampleData = reinterpret_cast<SoundSample*>(
		kgaAlloc(sampleDataAllocator, riffDataSize) );
	LeaveCriticalSection(&g_assetAllocationCsLock);
	if(!sampleData)
	{
		KLOG(ERROR, "Failed to allocate %i bytes for sample data!", 
		     riffDataSize);
		return {};
	}
	memcpy(sampleData, currByte, riffDataSize);
	// Assemble & return a valid result! //
	const RawSound result = {
		.sampleHz         = riffFmtSampleRate,
		.sampleBlockCount = riffDataSize / riffFmtNumChannels / 
		                      (riffFmtBitsPerSample/8),
		.bitsPerSample    = riffFmtBitsPerSample,
		.channelCount     = static_cast<u8>(riffFmtNumChannels),
		.sampleData       = sampleData,
	};
	return result;
}
internal PLATFORM_LOAD_WAV(platformLoadWav)
{
	// Load the entire WAV file into memory //
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, fileName);
	PlatformDebugReadFileResult file = platformReadEntireFile(szFileFullPath);
	if(!file.data)
	{
		KLOG(ERROR, "Failed to read entire file '%s'!", szFileFullPath);
		return {};
	}
	defer(platformFreeFileMemory(file.data));
	return decodeWav(file, sampleDataAllocator, szFileFullPath);
}
internal PLATFORM_IMGUI_ALLOC(platformImguiAlloc)
{
	return kgaAlloc(user_data, sz);
}
internal PLATFORM_IMGUI_FREE(platformImguiFree)
{
	kgaFree(user_data, ptr);
}
internal PLATFORM_DECODE_Z85_PNG(platformDecodeZ85Png)
{
	///TODO: use a separate allocator for Z85 decoding functionality
	const i32 tempFileBytes = kmath::safeTruncateI32(
		z85::decodedFileSizeBytes(z85PngNumBytes));
	i8*const tempFileData = reinterpret_cast<i8*>(
		kgaAlloc(g_genAllocStbImage, tempFileBytes));
	if(!tempFileData)
	{
		KLOG(ERROR, "Failed to allocate temp file data buffer!");
		return {};
	}
	defer(kgaFree(g_genAllocStbImage, tempFileData));
	if(!z85::decode(reinterpret_cast<const i8*>(z85PngData), tempFileData))
	{
		KLOG(ERROR, "z85::decode failure!");
		return {};
	}
	const PlatformDebugReadFileResult memorizedFile = {
		.dataBytes = kmath::safeTruncateU32(tempFileBytes),
		.data      = tempFileData };
	return decodePng(memorizedFile, pixelDataAllocator);
}
internal PLATFORM_DECODE_Z85_WAV(platformDecodeZ85Wav)
{
	///TODO: use a separate allocator for Z85 decoding functionality
	const i32 tempFileBytes = kmath::safeTruncateI32(
		z85::decodedFileSizeBytes(z85WavNumBytes));
	i8*const tempFileData = reinterpret_cast<i8*>(
		kgaAlloc(g_genAllocStbImage, tempFileBytes));
	if(!tempFileData)
	{
		KLOG(ERROR, "Failed to allocate temp file data buffer!");
		return {};
	}
	defer(kgaFree(g_genAllocStbImage, tempFileData));
	if(!z85::decode(reinterpret_cast<const i8*>(z85WavData), tempFileData))
	{
		KLOG(ERROR, "z85::decode failure!");
		return {};
	}
	const PlatformDebugReadFileResult memorizedFile = {
		.dataBytes = kmath::safeTruncateU32(tempFileBytes),
		.data      = tempFileData };
	return decodeWav(memorizedFile, sampleDataAllocator, TEXT("z85"));
}
internal PLATFORM_LOG(platformLog)
{
	SYSTEMTIME stLocalTime;
	GetLocalTime( &stLocalTime );
	TCHAR timeBuffer[80];
	StringCchPrintf(timeBuffer, CARRAY_COUNT(timeBuffer), 
#if INTERNAL_BUILD
	                TEXT("%02d,%03d"), 
	                stLocalTime.wSecond, stLocalTime.wMilliseconds );
#else
	                TEXT("%02d:%02d.%02d"), 
	                stLocalTime.wHour, stLocalTime.wMinute, 
	                stLocalTime.wSecond );
#endif// INTERNAL_BUILD
	// First, we build the new text to add to the log using a local buffer. //
	local_persist const size_t MAX_LOG_LINE_SIZE = 1024;
	char logLineBuffer[MAX_LOG_LINE_SIZE];
	const char*const strCategory = 
		   logCategory == PlatformLogCategory::K_INFO    ? "INFO"
		: (logCategory == PlatformLogCategory::K_WARNING ? "WARNING"
		: (logCategory == PlatformLogCategory::K_ERROR   ? "ERROR"
		: "UNKNOWN-CATEGORY"));
	// Print out the leading meta-data tag for this log entry //
	// god help us all if we ever have to deal with a 6-figure source file...
	if(!g_hasReceivedException)
	{
		kassert(sourceFileLineNumber >= 0 && sourceFileLineNumber < 100000);
	}
	const int logLineMetaCharsWritten = 
		_snprintf_s(logLineBuffer, MAX_LOG_LINE_SIZE, _TRUNCATE, 
		            "[%-7.7s|%s|%-15.15s:%-5i] ", strCategory, timeBuffer, 
		            sourceFileName, sourceFileLineNumber);
	if(!g_hasReceivedException)
	{
		kassert(logLineMetaCharsWritten > 0);
	}
	const size_t remainingLogLineSize = (logLineMetaCharsWritten < 0 ||
		logLineMetaCharsWritten >= MAX_LOG_LINE_SIZE) ? 0
		: MAX_LOG_LINE_SIZE - logLineMetaCharsWritten;
	int logLineCharsWritten = 0;
	if(remainingLogLineSize > 0)// should never fail, but I'm paranoid.
	// Print out the actual log entry into our temp line buffer //
	{
		va_list args;
		va_start(args, formattedString);
		logLineCharsWritten = 
			vsnprintf_s(logLineBuffer + logLineMetaCharsWritten, 
			            remainingLogLineSize, _TRUNCATE, formattedString, args);
		va_end(args);
		if(logLineCharsWritten < 0)
		{
			///TODO: handle overflow case for max characters per log line limit?
			fprintf_s(stderr, "[WARNING|%s:%i] LOG LINE OVERFLOW!\n",
			          __FILENAME__, __LINE__);
		}
	}
	// Now that we know what needs to be added to the log, we need to dump this
	//	line buffer into the appropriate log memory location.  First, 
	//	logBeginning gets filled up until it can no longer contain any more 
	//	lines.  For all lines afterwards, we use the logCircularBuffer. //
	// Duplicate log output to standard output streams //
	{
		FILE*const duplicatedOutput = 
			logCategory < PlatformLogCategory::K_WARNING
				? stdout : stderr;
		fprintf_s(duplicatedOutput, "%s\n", logLineBuffer);
#if SLOW_BUILD
		fflush(duplicatedOutput);
#endif
	}
	// Errors should NEVER happen.  Assert now to ensure that they are fixed as
	//	soon as possible!
	if(logCategory == PlatformLogCategory::K_ERROR && !g_hasReceivedException)
	{
		kassert(!"ERROR HAS BEEN LOGGED!");
	}
	//	Everything past this point modifies global log buffer state ////////////
	// Ensure writes to the global log buffer states are thread-safe! //
	EnterCriticalSection(&g_logCsLock);
	defer(LeaveCriticalSection(&g_logCsLock));
	// Calculate how big this new log line is, & which of the log buffers we 
	//	will write to //
	const size_t totalLogLineSize = 
		logLineMetaCharsWritten + logLineCharsWritten;
	const size_t remainingLogBeginning = 
		MAX_LOG_BUFFER_SIZE - g_logCurrentBeginningChar;
	// check for one extra character here because of the implicit "\n\0"
	if(remainingLogBeginning >= totalLogLineSize + 2)
	// append line to the log beginning //
	{
		const int charsWritten = 
			_snprintf_s(g_logBeginning + g_logCurrentBeginningChar,
			            remainingLogBeginning, _TRUNCATE, 
			            "%s\n", logLineBuffer);
		// we should have written the total log line + "\n". \0 isn't counted!
		if(!g_hasReceivedException)
		{
			kassert(charsWritten == totalLogLineSize + 1);
		}
		g_logCurrentBeginningChar += charsWritten;
	}
	else
	// append line to the log circular buffer //
	{
		// Ensure no more lines can be added to the log beginning buffer:
		g_logCurrentBeginningChar = MAX_LOG_BUFFER_SIZE;
		const size_t remainingLogCircularBuffer = 
			MAX_LOG_BUFFER_SIZE - g_logCurrentCircularBufferChar;
		if(!g_hasReceivedException)
		{
			kassert(remainingLogCircularBuffer > 0);
		}
		// I use `min` here as the second parameter to prevent _snprintf_s from 
		//	preemptively destroying earlier buffer data!
		_set_errno(0); int charsWritten = 
			_snprintf_s(g_logCircularBuffer + g_logCurrentCircularBufferChar,
			            // +2 here to account for the \n\0 at the end !!!
			            min(remainingLogCircularBuffer, totalLogLineSize + 2), 
			            _TRUNCATE, "%s\n", logLineBuffer);
		if(errno && !g_hasReceivedException)
		{
			const int error = errno;
			kassert(!"log circular buffer part 1 string print error!");
		}
		else if(charsWritten < 0)
		// If no errors have occurred, it means we have truncated the 
		//	string (circular buffer has been wrapped) //
		{
			// subtract one here because of the \0 that must be written!
			charsWritten = static_cast<int>(remainingLogCircularBuffer - 1);
		}
		g_logCurrentCircularBufferChar += charsWritten;
		// Attempt to wrap around the circular buffer if we reach the end //
		//@ASSUMPTION: the log circular buffer is >= the local log line buffer
		if(charsWritten == remainingLogCircularBuffer - 1)
		{
			if(!g_hasReceivedException)
			{
				kassert(g_logCurrentCircularBufferChar == 
				            MAX_LOG_BUFFER_SIZE - 1);
			}
			g_logCurrentCircularBufferChar = 0;
			// +2 here because we need to make sure to write the \n\0 at the end
			const size_t remainingLogLineBytes = 
				totalLogLineSize + 2 - charsWritten;
			if(remainingLogLineBytes == 1)
			{
				// if we only need to write a null-terminator, we shouldn't make
				//	another call to _snprintf_s requesting a `\n` write!
				*g_logCircularBuffer = '\0';
			}
			_set_errno(0);
			const int charsWrittenClipped = remainingLogLineBytes < 2 
				? static_cast<int>(remainingLogLineBytes)
				: _snprintf_s(g_logCircularBuffer, remainingLogLineBytes, 
				              _TRUNCATE, "%s\n", logLineBuffer + charsWritten);
			if(errno && !g_hasReceivedException)
			{
				kassert(!"log circular buffer part 2 string print error!");
			}
			if(!g_hasReceivedException)
			{
				kassert(charsWrittenClipped >= 0);
			}
			g_logCurrentCircularBufferChar += charsWrittenClipped;
#if INTERNAL_BUILD && 0
			// There should only ever be ONE \0 char in the circular buffer, 
			//	excluding the \0 which should always be present in the last 
			//	character position.  If this isn't true, then something has 
			//	seriously fucked up.
			u32 nullCharCount = 0;
			for(size_t c = 0; c < MAX_LOG_BUFFER_SIZE - 1; c++)
			{
				if(g_logCircularBuffer[c] == '\0')
					nullCharCount++;
				if(!g_hasReceivedException)
				{
					kassert(nullCharCount < 2);
				}
			}
#endif
		}
		// +1 here because of the '\n' at the end of our log line!
		g_logCircularBufferRunningTotal += totalLogLineSize + 1;
	}
}
internal void w32WriteLogToFile()
{
	local_persist const TCHAR fileNameLog[] = TEXT("log.txt");
	local_persist const DWORD MAX_DWORD = ~DWORD(1<<(sizeof(DWORD)*8-1));
	static_assert(MAX_LOG_BUFFER_SIZE < MAX_DWORD);
	///TODO: change this to use a platform-determined write directory
	TCHAR fullPathToNewLog[MAX_PATH] = {};
	TCHAR fullPathToLog   [MAX_PATH] = {};
	// determine full path to log files using platform-determined write 
	//	directory //
	{
		if(FAILED(StringCchPrintf(fullPathToNewLog, MAX_PATH, 
		                          TEXT("%s\\%s.new"), g_pathTemp, fileNameLog)))
		{
			KLOG(ERROR, "Failed to build fullPathToNewLog!  "
			     "g_pathTemp='%s' fileNameLog='%s'", 
			     g_pathTemp, fileNameLog);
			return;
		}
		if(FAILED(StringCchPrintf(fullPathToLog, MAX_PATH, 
		                          TEXT("%s\\%s"), g_pathTemp, fileNameLog)))
		{
			KLOG(ERROR, "Failed to build fullPathToLog!  "
			     "g_pathTemp='%s' fileNameLog='%s'", 
			     g_pathTemp, fileNameLog);
			return;
		}
	}
	const HANDLE fileHandle = 
		CreateFileA(fullPathToNewLog, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		///TODO: handle GetLastError
		kassert(!"Failed to create log file handle!");
		return;
	}
	// write the beginning log buffer //
	{
		const DWORD byteWriteCount = 
			static_cast<DWORD>(strnlen_s(g_logBeginning, MAX_LOG_BUFFER_SIZE));
		DWORD bytesWritten;
		if(!WriteFile(fileHandle, g_logBeginning, byteWriteCount, &bytesWritten, 
		              nullptr))
		{
			///TODO: handle GetLastError (close fileHandle)
			kassert(!"Failed to write log beginning!");
			return;
		}
		if(bytesWritten != byteWriteCount)
		{
			///TODO: handle this error case(close fileHandle)
			kassert(!"Failed to complete write log beginning!");
			return;
		}
	}
	// If the log's beginning buffer is full, that means we have stuff in the 
	//	circular buffer to write out.
	if(g_logCurrentBeginningChar == MAX_LOG_BUFFER_SIZE)
	{
		// If the total bytes written to the circular buffer exceed the buffer
		//	size, that means we have to indicate a discontinuity in the log file
		if(g_logCircularBufferRunningTotal >= MAX_LOG_BUFFER_SIZE - 1)
		{
			// write the log cut string //
			{
				local_persist const char CSTR_LOG_CUT[] = 
					"- - - - - - LOG CUT - - - - - - - -\n";
				const DWORD byteWriteCount = static_cast<DWORD>(
					strnlen_s(CSTR_LOG_CUT, sizeof(CSTR_LOG_CUT)));
				DWORD bytesWritten;
				if(!WriteFile(fileHandle, CSTR_LOG_CUT, byteWriteCount, 
				              &bytesWritten, nullptr))
				{
					///TODO: handle GetLastError (close fileHandle)
					kassert(!"Failed to write log cut string!");
					return;
				}
				if(bytesWritten != byteWriteCount)
				{
					///TODO: handle this error case (close fileHandle)
					kassert(!"Failed to complete write log cut string!");
					return;
				}
			}
			if(g_logCurrentCircularBufferChar < MAX_LOG_BUFFER_SIZE)
			// write the oldest contents of the circular buffer //
			{
				const char*const data = 
					g_logCircularBuffer + g_logCurrentCircularBufferChar + 1;
				const DWORD byteWriteCount = static_cast<DWORD>(
					strnlen_s(data, MAX_LOG_BUFFER_SIZE - 
					                g_logCurrentCircularBufferChar));
				DWORD bytesWritten;
				if(!WriteFile(fileHandle, data, byteWriteCount, 
				              &bytesWritten, nullptr))
				{
					///TODO: handle GetLastError (close fileHandle)
					kassert(!"Failed to write log circular buffer old!");
					return;
				}
				if(bytesWritten != byteWriteCount)
				{
					///TODO: handle this error case (close fileHandle)
					kassert(!"Failed to complete write circular buffer old!");
					return;
				}
			}
		}
		// write the newest contents of the circular buffer //
		{
			const DWORD byteWriteCount = static_cast<DWORD>(
				strnlen_s(g_logCircularBuffer, MAX_LOG_BUFFER_SIZE));
			DWORD bytesWritten;
			if(!WriteFile(fileHandle, g_logCircularBuffer, byteWriteCount, 
			              &bytesWritten, nullptr))
			{
				///TODO: handle GetLastError (close fileHandle)
				kassert(!"Failed to write log circular buffer new!");
				return;
			}
			if(bytesWritten != byteWriteCount)
			{
				///TODO: handle this error case (close fileHandle)
				kassert(!"Failed to complete write circular buffer new!");
				return;
			}
		}
	}
	if(!CloseHandle(fileHandle))
	{
		///TODO: handle GetLastError
		kassert(!"Failed to close new log file handle!");
		return;
	}
	// Copy the new log file into the finalized file name //
	if(!CopyFileA(fullPathToNewLog, fullPathToLog, false))
	{
		kassert(!"Failed to rename new log file!");
		return;
	}
	platformLog("win32-main", __LINE__, PlatformLogCategory::K_INFO,
	            "Log successfully wrote to '%s'!", fullPathToLog);
	// Once the log file has been finalized, we can delete the temporary file //
	if(!DeleteFileA(fullPathToNewLog))
	{
		platformLog("win32-main", __LINE__, PlatformLogCategory::K_WARNING,
		            "Failed to delete '%s'!", fullPathToNewLog);
	}
}
internal PLATFORM_READ_ENTIRE_FILE(platformReadEntireFile)
{
	PlatformDebugReadFileResult result = {};
	const HANDLE fileHandle = 
		CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, nullptr, 
		            OPEN_EXISTING, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		KLOG(ERROR, "Failed to create file handle! GetLastError=%i",
		     GetLastError());
		return result;
	}
	defer({
		if(!CloseHandle(fileHandle))
		{
			KLOG(ERROR, "Failed to close file handle! GetLastError=%i",
			     GetLastError());
			result.data = nullptr;
		}
	});
	u32 fileSize32;
	{
		LARGE_INTEGER fileSize;
		if(!GetFileSizeEx(fileHandle, &fileSize))
		{
			KLOG(ERROR, "Failed to get file size! GetLastError=%i",
			     GetLastError());
			return result;
		}
		fileSize32 = kmath::safeTruncateU32(fileSize.QuadPart);
		if(fileSize32 != fileSize.QuadPart)
		{
			KLOG(ERROR, "File size exceeds 32 bits!");
			return result;
		}
	}
	result.data = VirtualAlloc(nullptr, fileSize32, 
	                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if(!result.data)
	{
		KLOG(ERROR, "Failed to allocate %i bytes to read file! GetLastError=%i", 
		     fileSize32, GetLastError());
		return result;
	}
	defer({
		if(result.dataBytes != fileSize32 &&
			!VirtualFree(result.data, 0, MEM_RELEASE))
		{
			KLOG(ERROR, "Failed to VirtualFree! GetLastError=%i", 
			     GetLastError());
		}
		result.data = nullptr;
	});
	DWORD numBytesRead;
	if(!ReadFile(fileHandle, result.data, fileSize32, &numBytesRead, nullptr))
	{
		KLOG(ERROR, "Failed to read file! GetLastError=%i", 
		     fileSize32, GetLastError());
		return result;
	}
	if(numBytesRead != fileSize32)
	{
		KLOG(ERROR, "Failed to read file! bytes read: %i / %i", 
		     numBytesRead, fileSize32);
		return result;
	}
	result.dataBytes = fileSize32;
	return result;
}
internal PLATFORM_FREE_FILE_MEMORY(platformFreeFileMemory)
{
	if(!VirtualFree(fileMemory, 0, MEM_RELEASE))
	{
		KLOG(ERROR, "Failed to free file memory! GetLastError=%i", 
		     GetLastError());
	}
}
PLATFORM_WRITE_ENTIRE_FILE(platformWriteEntireFile)
{
	const HANDLE fileHandle = 
		CreateFileA(fileName, GENERIC_WRITE, 0, nullptr, 
		            CREATE_ALWAYS, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		KLOG(ERROR, "Failed to create write file handle '%s'! GetLastError=%i", 
		     fileName, GetLastError());
		return false;
	}
	bool result = true;
	defer({
		if(!CloseHandle(fileHandle))
		{
			KLOG(ERROR, "Failed to close file handle '%s'! GetLastError=%i", 
			     fileName, GetLastError());
			result = false;
		}
	});
	DWORD bytesWritten;
	if(!WriteFile(fileHandle, data, dataBytes, &bytesWritten, nullptr))
	{
		KLOG(ERROR, "Failed to write to file '%s'! GetLastError=%i", 
		     fileName, GetLastError());
		return false;
	}
	if(bytesWritten != dataBytes)
	{
		KLOG(ERROR, "Failed to completely write to file '%s'! "
		     "Bytes written: %i / %i", 
		     fileName, bytesWritten, dataBytes);
		return false;
	}
	return result;
}
internal PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(w32GetGamePadActiveButton)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_gamePadArrayA));
	if(gamePadIndex < XUSER_MAX_COUNT)
	/* the gamepad is an Xinput controller */
	{
		return w32XInputGetGamePadActiveButton(gamePadIndex);
	}
	else
	/* gamepad is DirectInput */
	{
		return w32DInputGetGamePadActiveButton(gamePadIndex - XUSER_MAX_COUNT);
	}
}
internal PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(w32GetGamePadActiveAxis)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_gamePadArrayA));
	if(gamePadIndex < XUSER_MAX_COUNT)
	/* the gamepad is an Xinput controller */
	{
		return w32XInputGetGamePadActiveAxis(gamePadIndex);
	}
	else
	/* gamepad is DirectInput */
	{
		return w32DInputGetGamePadActiveAxis(gamePadIndex - XUSER_MAX_COUNT);
	}
}
internal PLATFORM_GET_GAME_PAD_PRODUCT_NAME(w32GetGamePadProductName)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_gamePadArrayA));
	if(gamePadIndex < XUSER_MAX_COUNT)
	/* Xinput controller */
	{
		StringCchPrintf(o_buffer, bufferSize, TEXT("XInput Controller"));
	}
	/* DirectInput controller */
	else
	{
		w32DInputGetGamePadProductName(gamePadIndex - XUSER_MAX_COUNT, o_buffer, 
		                               bufferSize);
	}
}
internal PLATFORM_GET_GAME_PAD_PRODUCT_GUID(w32GetGamePadProductGuid)
{
	kassert(gamePadIndex < CARRAY_COUNT(g_gamePadArrayA));
	if(gamePadIndex < XUSER_MAX_COUNT)
	/* Xinput controller */
	{
		StringCchPrintf(o_buffer, bufferSize, TEXT("N/A"));
	}
	/* DirectInput controller */
	else
	{
		w32DInputGetGamePadProductGuid(gamePadIndex - XUSER_MAX_COUNT, o_buffer, 
		                               bufferSize);
	}
}
GAME_INITIALIZE(gameInitializeStub)
{
}
GAME_ON_RELOAD_CODE(gameOnReloadCodeStub)
{
}
GAME_RENDER_AUDIO(gameRenderAudioStub)
{
}
GAME_UPDATE_AND_DRAW(gameUpdateAndDrawStub)
{
	// continue running the application to keep attempting to reload game code!
	return true;
}
internal FILETIME w32GetLastWriteTime(const char* fileName)
{
	FILETIME result = {};
	WIN32_FILE_ATTRIBUTE_DATA fileAttributeData;
	if(!GetFileAttributesExA(fileName, GetFileExInfoStandard, 
	                         &fileAttributeData))
	{
		KLOG(WARNING, "Failed to get last write time of file '%s'! "
		     "GetLastError=%i", fileName, GetLastError());
		return result;
	}
	result = fileAttributeData.ftLastWriteTime;
	return result;
}
internal PLATFORM_GET_ASSET_WRITE_TIME(platformGetAssetWriteTime)
{
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, assetFileName);
	FILETIME fileWriteTime = w32GetLastWriteTime(szFileFullPath);
	ULARGE_INTEGER largeInt;
	largeInt.HighPart = fileWriteTime.dwHighDateTime;
	largeInt.LowPart  = fileWriteTime.dwLowDateTime;
	return largeInt.QuadPart;
}
internal PLATFORM_IS_ASSET_CHANGED(platformIsAssetChanged)
{
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, assetFileName);
	FILETIME fileWriteTime = w32GetLastWriteTime(szFileFullPath);
	ULARGE_INTEGER largeInt;
	largeInt.QuadPart = lastWriteTime;
	FILETIME fileWriteTimePrev;
	fileWriteTimePrev.dwHighDateTime = largeInt.HighPart;
	fileWriteTimePrev.dwLowDateTime  = largeInt.LowPart;
	return CompareFileTime(&fileWriteTime, &fileWriteTimePrev) != 0;
}
internal PLATFORM_IS_ASSET_AVAILABLE(platformIsAssetAvailable)
{
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, assetFileName);
	// Check to see if the file is open exclusively by another program by 
	//	attempting to open it in exclusive mode.
	// https://stackoverflow.com/a/12821767
	const HANDLE hFile = CreateFileA(szFileFullPath, GENERIC_READ, 0, nullptr, 
	                                 OPEN_EXISTING, 0, NULL);
	defer(CloseHandle(hFile));
	if(hFile && hFile != INVALID_HANDLE_VALUE)
	{
		return true;
	}
	return false;
}
internal GameCode w32LoadGameCode(const char* fileNameDll, 
                                  const char* fileNameDllTemp)
{
	GameCode result = {};
	result.initialize       = gameInitializeStub;
	result.onReloadCode     = gameOnReloadCodeStub;
	result.renderAudio      = gameRenderAudioStub;
	result.updateAndDraw    = gameUpdateAndDrawStub;
	result.dllLastWriteTime = w32GetLastWriteTime(fileNameDll);
	if(result.dllLastWriteTime.dwHighDateTime == 0 &&
		result.dllLastWriteTime.dwLowDateTime == 0)
	{
		return result;
	}
	if(!CopyFileA(fileNameDll, fileNameDllTemp, false))
	{
		KLOG(WARNING, "Failed to copy file '%s' to '%s'! GetLastError=%i", 
		     fileNameDll, fileNameDllTemp, GetLastError());
	}
	else
	{
		result.hLib = LoadLibraryA(fileNameDllTemp);
		if(!result.hLib)
		{
			KLOG(ERROR, "Failed to load library '%s'! GetLastError=%i", 
			     fileNameDllTemp, GetLastError());
		}
	}
	if(result.hLib)
	{
		result.initialize = reinterpret_cast<fnSig_gameInitialize*>(
			GetProcAddress(result.hLib, "gameInitialize"));
		if(!result.initialize)
		{
			KLOG(ERROR, "Failed to get proc address 'gameInitialize'! "
			     "GetLastError=%i", GetLastError());
		}
		result.onReloadCode = reinterpret_cast<fnSig_gameOnReloadCode*>(
			GetProcAddress(result.hLib, "gameOnReloadCode"));
		if(!result.onReloadCode)
		{
			// This is only a warning because the game can optionally just not
			//	implement the hot-reload callback. //
			///TODO: detect if `GameCode` has the ability to hot-reload and 
			///      don't perform hot-reloading if this is not the case.
			KLOG(WARNING, "Failed to get proc address 'gameOnReloadCode'! "
			     "GetLastError=%i", GetLastError());
		}
		result.renderAudio = reinterpret_cast<fnSig_gameRenderAudio*>(
			GetProcAddress(result.hLib, "gameRenderAudio"));
		if(!result.renderAudio)
		{
			KLOG(ERROR, "Failed to get proc address 'gameRenderAudio'! "
			     "GetLastError=%i", GetLastError());
		}
		result.updateAndDraw = reinterpret_cast<fnSig_gameUpdateAndDraw*>(
			GetProcAddress(result.hLib, "gameUpdateAndDraw"));
		if(!result.updateAndDraw)
		{
			KLOG(ERROR, "Failed to get proc address 'gameUpdateAndDraw'! "
			     "GetLastError=%i", GetLastError());
		}
		result.isValid = (result.initialize && result.renderAudio && 
		                  result.updateAndDraw);
	}
	if(!result.isValid)
	{
		KLOG(WARNING, "Game code is invalid!");
		result.initialize    = gameInitializeStub;
		result.onReloadCode  = gameOnReloadCodeStub;
		result.renderAudio   = gameRenderAudioStub;
		result.updateAndDraw = gameUpdateAndDrawStub;
	}
	return result;
}
internal void w32UnloadGameCode(GameCode& gameCode)
{
	if(gameCode.hLib)
	{
		if(!FreeLibrary(gameCode.hLib))
		{
			KLOG(ERROR, "Failed to free game code dll! GetLastError=", 
			     GetLastError());
		}
		gameCode.hLib = NULL;
	}
	gameCode.isValid = false;
	gameCode.renderAudio   = gameRenderAudioStub;
	gameCode.updateAndDraw = gameUpdateAndDrawStub;
}
internal W32Dimension2d w32GetWindowDimensions(HWND hwnd)
{
	RECT clientRect;
	if(GetClientRect(hwnd, &clientRect))
	{
		const u32 w = clientRect.right  - clientRect.left;
		const u32 h = clientRect.bottom - clientRect.top;
		return W32Dimension2d{.width=w, .height=h};
	}
	else
	{
		KLOG(ERROR, "Failed to get window dimensions! GetLastError=%i", 
		     GetLastError());
		return W32Dimension2d{.width=0, .height=0};
	}
}
internal ButtonState* w32DecodeVirtualKey(GameKeyboard* gk, WPARAM vKeyCode)
{
	ButtonState* buttonState = nullptr;
	// Virtual-Key Codes: 
	//	https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	switch(vKeyCode)
	{
		case VK_OEM_COMMA:  buttonState = &gk->comma; break;
		case VK_OEM_PERIOD: buttonState = &gk->period; break;
		case VK_OEM_2:      buttonState = &gk->slashForward; break;
		case VK_OEM_5:      buttonState = &gk->slashBack; break;
		case VK_OEM_4:      buttonState = &gk->curlyBraceLeft; break;
		case VK_OEM_6:      buttonState = &gk->curlyBraceRight; break;
		case VK_OEM_1:      buttonState = &gk->semicolon; break;
		case VK_OEM_7:      buttonState = &gk->quote; break;
		case VK_OEM_3:      buttonState = &gk->grave; break;
		case VK_OEM_MINUS:  buttonState = &gk->tenkeyless_minus; break;
		case VK_OEM_PLUS:   buttonState = &gk->equals; break;
		case VK_BACK:       buttonState = &gk->backspace; break;
		case VK_ESCAPE:     buttonState = &gk->escape; break;
		case VK_RETURN:     buttonState = &gk->enter; break;
		case VK_SPACE:      buttonState = &gk->space; break;
		case VK_TAB:        buttonState = &gk->tab; break;
		case VK_LSHIFT:     buttonState = &gk->shiftLeft; break;
		case VK_RSHIFT:     buttonState = &gk->shiftRight; break;
		case VK_LCONTROL:   buttonState = &gk->controlLeft; break;
		case VK_RCONTROL:   buttonState = &gk->controlRight; break;
		case VK_LMENU:      buttonState = &gk->altLeft; break;
		case VK_RMENU:      buttonState = &gk->altRight; break;
		case VK_UP:         buttonState = &gk->arrowUp; break;
		case VK_DOWN:       buttonState = &gk->arrowDown; break;
		case VK_LEFT:       buttonState = &gk->arrowLeft; break;
		case VK_RIGHT:      buttonState = &gk->arrowRight; break;
		case VK_INSERT:     buttonState = &gk->insert; break;
		case VK_DELETE:     buttonState = &gk->deleteKey; break;
		case VK_HOME:       buttonState = &gk->home; break;
		case VK_END:        buttonState = &gk->end; break;
		case VK_PRIOR:      buttonState = &gk->pageUp; break;
		case VK_NEXT:       buttonState = &gk->pageDown; break;
		case VK_DECIMAL:    buttonState = &gk->numpad_period; break;
		case VK_DIVIDE:     buttonState = &gk->numpad_divide; break;
		case VK_MULTIPLY:   buttonState = &gk->numpad_multiply; break;
		case VK_SEPARATOR:  buttonState = &gk->numpad_minus; break;
		case VK_ADD:        buttonState = &gk->numpad_add; break;
		default:
		{
			if(vKeyCode >= 0x41 && vKeyCode <= 0x5A)
			{
				buttonState = &gk->a + (vKeyCode - 0x41);
			}
			else if(vKeyCode >= 0x30 && vKeyCode <= 0x39)
			{
				buttonState = &gk->tenkeyless_0 + (vKeyCode - 0x30);
			}
			else if(vKeyCode >= 0x70 && vKeyCode <= 0x7B)
			{
				buttonState = &gk->f1 + (vKeyCode - 0x70);
			}
			else if(vKeyCode >= 0x60 && vKeyCode <= 0x69)
			{
				buttonState = &gk->numpad_0 + (vKeyCode - 0x60);
			}
		} break;
	}
	return buttonState;
}
internal void w32GetKeyboardKeyStates(GameKeyboard* gkCurrFrame, 
                                      GameKeyboard* gkPrevFrame)
{
	for(WPARAM vKeyCode = 0; vKeyCode <= 0xFF; vKeyCode++)
	{
		ButtonState* buttonState = w32DecodeVirtualKey(gkCurrFrame, vKeyCode);
		if(buttonState)
		{
			const SHORT asyncKeyState = 
				GetAsyncKeyState(static_cast<int>(vKeyCode));
			// do NOT use the least-significant bit to determine key state!
			//	Why? See documentation on the GetAsyncKeyState function:
			// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate
			const bool keyDown = (asyncKeyState & ~1) != 0;
			const size_t buttonIndex = buttonState - gkCurrFrame->vKeys;
			*buttonState = keyDown
				? (gkPrevFrame->vKeys[buttonIndex] >= ButtonState::PRESSED
					? ButtonState::HELD
					: ButtonState::PRESSED)
				: ButtonState::NOT_PRESSED;
		}
	}
	// update the game keyboard modifier states //
	gkCurrFrame->modifiers.shift =
		(gkCurrFrame->shiftLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->shiftRight > ButtonState::NOT_PRESSED);
	gkCurrFrame->modifiers.control =
		(gkCurrFrame->controlLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->controlRight > ButtonState::NOT_PRESSED);
	gkCurrFrame->modifiers.alt =
		(gkCurrFrame->altLeft  > ButtonState::NOT_PRESSED ||
		 gkCurrFrame->altRight > ButtonState::NOT_PRESSED);
}
internal DWORD w32QueryNearestMonitorRefreshRate(HWND hwnd)
{
	local_persist const DWORD DEFAULT_RESULT = 60;
	const HMONITOR nearestMonitor = 
		MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEXA monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFOEXA);
	if(!GetMonitorInfoA(nearestMonitor, &monitorInfo))
	{
		KLOG(ERROR, "Failed to get monitor info!");
		return DEFAULT_RESULT;
	}
	DEVMODEA monitorDevMode;
	monitorDevMode.dmSize = sizeof(DEVMODEA);
	monitorDevMode.dmDriverExtra = 0;
	if(!EnumDisplaySettingsA(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, 
	                         &monitorDevMode))
	{
		KLOG(ERROR, "Failed to enum display settings!");
		return DEFAULT_RESULT;
	}
	if(monitorDevMode.dmDisplayFrequency < 2)
	{
		KLOG(WARNING, "Unknown hardware-defined refresh rate! "
		     "Defaulting to %ihz...", DEFAULT_RESULT);
		return DEFAULT_RESULT;
	}
	return monitorDevMode.dmDisplayFrequency;
}
internal LRESULT CALLBACK w32MainWindowCallback(HWND hwnd, UINT uMsg, 
                                                WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
	LRESULT result = 0;
	switch(uMsg)
	{
		case WM_SYSCOMMAND:
		{
			KLOG(INFO, "WM_SYSCOMMAND: type=0x%x", wParam);
			if(wParam == SC_KEYMENU)
			// These window messages need to be captured in order to stop 
			//	windows from needlessly beeping when we use ALT+KEY combinations 
			{
				break;
			}
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		}break;
#if 0
		case WM_MENUCHAR:
		{
			KLOG(INFO, "WM_MENUCHAR");
			result = MAKELRESULT(0, MNC_IGNORE);
		}break;
#endif // 0
		case WM_SETCURSOR:
		{
			HCURSOR cursor = NULL;
			switch(LOWORD(lParam))
			{
				case HTBOTTOM:
				case HTTOP:
					cursor = g_cursorSizeVertical;
				break;
				case HTLEFT:
				case HTRIGHT:
					cursor = g_cursorSizeHorizontal;
				break;
				case HTBOTTOMLEFT:
				case HTTOPRIGHT:
					cursor = g_cursorSizeNeSw;
				break;
				case HTBOTTOMRIGHT:
				case HTTOPLEFT:
					cursor = g_cursorSizeNwSe;
				break;
				case HTCLIENT:
					if(!g_displayCursor)
					{
						break;
					}
				default:
					cursor = g_cursorArrow;
				break;
			}
			SetCursor(cursor);
		} break;
		case WM_SIZE:
		{
			KLOG(INFO, "WM_SIZE: type=%i area={%i,%i}", 
			     wParam, LOWORD(lParam), HIWORD(lParam));
		} break;
		case WM_DESTROY:
		{
			///TODO: handle this error: recreate window?
			g_running = false;
			KLOG(INFO, "WM_DESTROY");
		} break;
		case WM_CLOSE:
		{
			///TODO: ask user first before destroying
			g_running = false;
			KLOG(INFO, "WM_CLOSE");
		} break;
		case WM_ACTIVATEAPP:
		{
			g_isFocused = wParam;
			KLOG(INFO, "WM_ACTIVATEAPP: activated=%s threadId=%i",
			     (wParam ? "TRUE" : "FALSE"), lParam);
		} break;
#if 0
		case WM_PAINT:
		{
			PAINTSTRUCT paintStruct;
			const HDC hdc = BeginPaint(hwnd, &paintStruct);
			if(hdc)
			{
				const int w = 
					paintStruct.rcPaint.right  - paintStruct.rcPaint.left;
				const int h = 
					paintStruct.rcPaint.bottom - paintStruct.rcPaint.top;
				const W32Dimension2d winDims = w32GetWindowDimensions(hwnd);
				w32UpdateWindow(g_backBuffer, hdc, 
				                winDims.width, winDims.height);
			}
			EndPaint(hwnd, &paintStruct);
		} break;
#endif
		case WM_DEVICECHANGE:
		{
			KLOG(INFO, "WM_DEVICECHANGE: event=0x%x", wParam);
			switch(wParam)
			{
				case DBT_DEVNODES_CHANGED:
				{
					KLOG(INFO, "\tA device has been added or removed!");
					g_deviceChangeDetected = true;
				} break;
			}
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		} break;
		default:
		{
#if 0
			KLOG(INFO, "Window Message uMsg==0x%x", uMsg);
#endif // 0
			result = DefWindowProc(hwnd, uMsg, wParam, lParam);
		}break;
	}
	return result;
}
internal LARGE_INTEGER w32QueryPerformanceCounter()
{
	LARGE_INTEGER result;
	if(!QueryPerformanceCounter(&result))
	{
		KLOG(ERROR, "Failed to query performance counter! GetLastError=%i", 
		     GetLastError());
	}
	return result;
}
internal f32 w32ElapsedSeconds(const LARGE_INTEGER& previousPerformanceCount, 
                               const LARGE_INTEGER& performanceCount)
{
	kassert(performanceCount.QuadPart > previousPerformanceCount.QuadPart);
	const LONGLONG perfCountDiff = 
		performanceCount.QuadPart - previousPerformanceCount.QuadPart;
	const f32 elapsedSeconds = 
		static_cast<f32>(perfCountDiff) / g_perfCounterHz.QuadPart;
	return elapsedSeconds;
}
internal int w32GenerateDump(PEXCEPTION_POINTERS pExceptionPointers)
{
	// copy-pasta from MSDN basically:
	//	https://docs.microsoft.com/en-us/windows/win32/dxtecharts/crash-dump-analysis
	///TODO: maybe make this a little more robust in the future...
	TCHAR szFileName[MAX_PATH]; 
	SYSTEMTIME stLocalTime;
	GetLocalTime( &stLocalTime );
	// Create a companion folder to store PDB files specifically for this 
	//	dump! //
	TCHAR szPdbDirectory[MAX_PATH];
	StringCchPrintf(szPdbDirectory, MAX_PATH, 
				TEXT("%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld"), 
				g_pathTemp, APPLICATION_VERSION, 
				stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, 
				stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, 
				GetCurrentProcessId(), GetCurrentThreadId());
	if(!CreateDirectory(szPdbDirectory, NULL))
	{
		platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
					"Failed to create dir '%s'!  GetLastError=%i",
					szPdbDirectory, GetLastError());
		return EXCEPTION_EXECUTE_HANDLER;
	}
	// Create the mini dump! //
	StringCchPrintf(szFileName, MAX_PATH, 
	                TEXT("%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp"), 
	                szPdbDirectory, APPLICATION_VERSION, 
	                stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, 
	                stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, 
	                GetCurrentProcessId(), GetCurrentThreadId());
	const HANDLE hDumpFile = CreateFile(szFileName, GENERIC_READ|GENERIC_WRITE, 
	                                    FILE_SHARE_WRITE|FILE_SHARE_READ, 
	                                    0, CREATE_ALWAYS, 0, 0);
	MINIDUMP_EXCEPTION_INFORMATION ExpParam = {
		.ThreadId          = GetCurrentThreadId(),
		.ExceptionPointers = pExceptionPointers,
		.ClientPointers    = TRUE,
	};
	const BOOL bMiniDumpSuccessful = 
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
		                  hDumpFile, MiniDumpWithDataSegs, &ExpParam, 
		                  NULL, NULL);
	if(bMiniDumpSuccessful)
	{
		g_hasWrittenCrashDump = true;
#if defined(UNICODE) || defined(_UNICODE)
		char ansiFileName[MAX_PATH];
		size_t convertedCharCount;
		wcstombs_s(&convertedCharCount, ansiFileName, sizeof(ansiFileName), 
		           szFileName, sizeof(szFileName));
		///TODO: U16 platformLog
#else
		platformLog("win32-main", __LINE__, PlatformLogCategory::K_INFO,
		            "Successfully wrote mini dump to: '%s'", szFileName);
#endif
		// Attempt to copy the win32 application's pdb file to the dump 
		//	location //
		TCHAR szFileNameCopySource[MAX_PATH];
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s.pdb"),
		                g_pathToExe, APPLICATION_NAME);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s.pdb"), 
		                szPdbDirectory, APPLICATION_NAME);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		/* Attempt to copy the win32 application's VC_*.pdb file to the dump 
			location */
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\VC_%s.pdb"),
		                g_pathToExe, APPLICATION_NAME);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\VC_%s.pdb"), 
		                szPdbDirectory, APPLICATION_NAME);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		// Find the most recent game*.pdb file, then place the filename into 
		//	`szFileNameCopySource` //
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s*.pdb"),
		                g_pathToExe, FILE_NAME_GAME_DLL);
		WIN32_FIND_DATA findFileData;
		HANDLE findHandleGameDll = 
			FindFirstFile(szFileNameCopySource, &findFileData);
		if(findHandleGameDll == INVALID_HANDLE_VALUE)
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to begin search for '%s'!  GetLastError=%i",
			            szFileNameCopySource, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		FILETIME creationTimeLatest = findFileData.ftCreationTime;
		TCHAR fileNameGamePdb[MAX_PATH];
		StringCchPrintf(fileNameGamePdb, MAX_PATH, TEXT("%s"),
		                findFileData.cFileName);
		while(BOOL findNextFileResult = 
			FindNextFile(findHandleGameDll, &findFileData))
		{
			if(!findNextFileResult && GetLastError() != ERROR_NO_MORE_FILES)
			{
				platformLog("win32-main", __LINE__, 
				            PlatformLogCategory::K_ERROR,
				            "Failed to find next for '%s'!  GetLastError=%i",
				            szFileNameCopySource, GetLastError());
				return EXCEPTION_EXECUTE_HANDLER;
			}
			if(CompareFileTime(&findFileData.ftCreationTime, 
			                   &creationTimeLatest) > 0)
			{
				creationTimeLatest = findFileData.ftCreationTime;
				StringCchPrintf(fileNameGamePdb, MAX_PATH, TEXT("%s"),
				                findFileData.cFileName);
			}
		}
		if(!FindClose(findHandleGameDll))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to close search for '%s*.pdb'!  "
			            "GetLastError=%i",
			            FILE_NAME_GAME_DLL, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		// attempt to copy game's pdb file //
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s"),
		                g_pathToExe, fileNameGamePdb);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s"), 
		                szPdbDirectory, fileNameGamePdb);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		/* attempt to copy the game's VC_*.pdb file */
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\VC_%s"),
		                g_pathToExe, fileNameGamePdb);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\VC_%s"), 
		                szPdbDirectory, fileNameGamePdb);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
		// Attempt to copy the win32 EXE file into the dump location //
		StringCchPrintf(szFileNameCopySource, MAX_PATH, TEXT("%s\\%s.exe"),
		                g_pathToExe, APPLICATION_NAME);
		StringCchPrintf(szFileName, MAX_PATH, TEXT("%s\\%s.exe"), 
		                szPdbDirectory, APPLICATION_NAME);
		if(!CopyFile(szFileNameCopySource, szFileName, false))
		{
			platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
			            "Failed to copy '%s' to '%s'!  GetLastError=%i",
			            szFileNameCopySource, szFileName, GetLastError());
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}
    return EXCEPTION_EXECUTE_HANDLER;
}
internal LONG WINAPI w32VectoredExceptionHandler(
                                             PEXCEPTION_POINTERS pExceptionInfo)
{
	g_hasReceivedException = true;
	if(!g_hasWrittenCrashDump)
	{
		w32GenerateDump(pExceptionInfo);
	}
	// break debugger to give us a chance to figure out what the hell happened
	if(IsDebuggerPresent())
	{
		DebugBreak();
	}
	// I don't use the KLOG macro here because `strrchr` from the 
	//	__FILENAME__ macro seems to just break everything :(
	platformLog("win32-main", __LINE__, PlatformLogCategory::K_ERROR,
	            "Vectored Exception! 0x%x", 
	            pExceptionInfo->ExceptionRecord->ExceptionCode);
	w32WriteLogToFile();
	///TODO: cleanup system-wide settings/handles, such as the OS timer 
	///      granularity setting!
	ExitProcess(0xDEADC0DE);
	// return EXCEPTION_CONTINUE_SEARCH;
}
#if 0
internal LONG WINAPI w32TopLevelExceptionHandler(
                                             PEXCEPTION_POINTERS pExceptionInfo)
{
	g_hasReceivedException = true;
	fprintf_s(stderr, "Exception! 0x%x\n", 
	          pExceptionInfo->ExceptionRecord->ExceptionCode);
	fflush(stderr);
	//KLOG(ERROR, "Exception! 0x%x", 
	//     pExceptionInfo->ExceptionRecord->ExceptionCode);
	w32WriteLogToFile();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
DWORD WINAPI w32WorkThread(_In_ LPVOID lpParameter)
{
	W32ThreadInfo*const threadInfo = 
		reinterpret_cast<W32ThreadInfo*>(lpParameter);
	KLOG(INFO, "thread[%i]: started!", threadInfo->index);
	while(true)
	{
		if(!jobQueuePerformWork(threadInfo->jobQueue, threadInfo->index))
		{
			jobQueueWaitForWork(threadInfo->jobQueue);
		}
	}
	return 0;
}
#include <hidsdi.h>
internal bool w32RawInputEnumerateDevices()
{
	RAWINPUTDEVICELIST rawInputDeviceList[64];
	UINT rawInputDeviceBufferSize = CARRAY_COUNT(rawInputDeviceList);
	const UINT numDevices = 
		GetRawInputDeviceList(rawInputDeviceList, 
		                      &rawInputDeviceBufferSize, 
		                      sizeof(rawInputDeviceList[0]));
	if(numDevices == (UINT)-1)
	{
		KLOG(ERROR, "Failed to get RawInput device list! getlasterror=%i",
		     GetLastError());
		return false;
	}
	for(UINT rid = 0; rid < numDevices; rid++)
	{
		/* get the raw input device's registry address */
		TCHAR deviceNameBuffer[256];
		UINT deviceNameBufferSize = CARRAY_COUNT(deviceNameBuffer);
		const UINT deviceNameCharacterCount = 
			GetRawInputDeviceInfo(rawInputDeviceList[rid].hDevice,
			                      RIDI_DEVICENAME, deviceNameBuffer, 
			                      &deviceNameBufferSize);
		if(deviceNameCharacterCount < 0)
		{
			KLOG(ERROR, "Failed to get RawInput device name! getlasterror=%i", 
			     GetLastError());
			return false;
		}
		/* get the raw input device's info */
		RID_DEVICE_INFO rawInputDeviceInfo = {};
		rawInputDeviceInfo.cbSize = sizeof(rawInputDeviceInfo);
		UINT deviceInfoSize       = sizeof(rawInputDeviceInfo);
		const UINT deviceInfoByteCount = 
			GetRawInputDeviceInfo(rawInputDeviceList[rid].hDevice,
			                      RIDI_DEVICEINFO, &rawInputDeviceInfo, 
			                      &deviceInfoSize);
		if(deviceInfoByteCount != sizeof(rawInputDeviceInfo))
		{
			KLOG(ERROR, "Failed to get RawInput device info! getlasterror=%i", 
			     GetLastError());
			return false;
		}
		switch(rawInputDeviceInfo.dwType)
		{
			case RIM_TYPEMOUSE:
			case RIM_TYPEKEYBOARD:
			{
				/* if this is a mouse/keyboard, just ignore it */
				continue;
			}break;
			case RIM_TYPEHID:
			{
				/* only certain types of devices can be opened with shared 
					access mode.  If a device can't be opened with shared 
					access, then there isn't really a point in continuing since 
					we can't even query the device's product name.  For info on 
					what devices can be opened in shared access, see:
					https://docs.microsoft.com/en-us/windows-hardware/drivers/hid/hid-clients-supported-in-windows */
				if(!(rawInputDeviceInfo.hid.usUsagePage == 0x1
					&& (rawInputDeviceInfo.hid.usUsage == 0x4
						|| rawInputDeviceInfo.hid.usUsage == 0x5)))
				// if it's anything that isn't a game controller
				{
					/* just skip it because it's likely not allowed to be opened 
						in shared mode anyways */
					continue;
				}
				/* get the product name of the HID */
				HANDLE hHid = CreateFile(deviceNameBuffer, 
				                         GENERIC_READ | GENERIC_WRITE, 
				                         FILE_SHARE_READ | FILE_SHARE_WRITE, 
				                         NULL, OPEN_EXISTING, NULL, NULL);
				if(hHid == INVALID_HANDLE_VALUE)
				{
					KLOG(ERROR, "Failed to open handle to HID! getlasterror=%i", 
					     GetLastError());
					return false;
				}
				defer(CloseHandle(hHid));
				WCHAR deviceProductNameBuffer[256];
				if(!HidD_GetProductString(hHid, deviceProductNameBuffer, 
				                          sizeof(deviceProductNameBuffer)))
				{
					KLOG(ERROR, "Failed to get HID product string! "
					     "getlasterror=%i", GetLastError());
					return false;
				}
				KLOG(INFO, "--- RawInput device [%i] (Game Controller) ---", 
				     rid);
				KLOG(INFO, "\tdeviceName='%s'", deviceNameBuffer);
				KLOG(INFO, "\tproductString='%ws'", deviceProductNameBuffer);
			}break;
		}
	}
	return true;
}
extern int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, 
                           PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
	local_persist const int RETURN_CODE_SUCCESS = 0;
	///TODO: OR the failure code with a more specific # to indicate why it 
	///      failed probably?
	local_persist const int RETURN_CODE_FAILURE = 0xBADC0DE0;
	// Initialize locks to ensure thread safety for various systems //
	{
		InitializeCriticalSection(&g_logCsLock);
		InitializeCriticalSection(&g_assetAllocationCsLock);
		InitializeCriticalSection(&g_vorbisAllocationCsLock);
		InitializeCriticalSection(&g_stbiAllocationCsLock);
	}
	// Reserve stack space for stack overflow exceptions //
	{
		// Experimentally, this is about the minimum # of reserved stack bytes
		//	required to carry out my debug dump/log routines when a stack 
		//	overflow exception occurs.
		local_persist const ULONG DESIRED_RESERVED_STACK_BYTES = 
			static_cast<ULONG>(kmath::kilobytes(16));
		ULONG stackSizeInBytes = DESIRED_RESERVED_STACK_BYTES;
		if(SetThreadStackGuarantee(&stackSizeInBytes))
		{
			KLOG(INFO, "Previous reserved stack=%ld, new reserved stack=%ld", 
			     stackSizeInBytes, DESIRED_RESERVED_STACK_BYTES);
		}
		else
		{
			KLOG(WARNING, "Failed to set & retrieve reserved stack size!  "
			     "The system probably won't be able to log stack "
			     "overflow exceptions.");
		}
	}
#if 0
	if(SetUnhandledExceptionFilter(w32TopLevelExceptionHandler))
	{
		KLOG(INFO, "Replaced top level exception filter!");
	}
	else
	{
		KLOG(INFO, "Set new top level exception filter!");
	}
#endif
	if(!AddVectoredExceptionHandler(1, w32VectoredExceptionHandler))
	{
		KLOG(WARNING, "Failed to add vectored exception handler!");
	}
	defer(w32WriteLogToFile());
	KLOG(INFO, "START!");
	if(!w32RawInputEnumerateDevices())
	{
		KLOG(ERROR, "Failed to enumerate raw input devices!");
		return RETURN_CODE_FAILURE;
	}
	// parse command line arguments //
	WCHAR relativeAssetDir[MAX_PATH];
	relativeAssetDir[0] = '\0';
	{
		int argc;
		LPWSTR*const argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if(argv)
		{
			for(int arg = 0; arg < argc; arg++)
			{
				KLOG(INFO, "arg[%i]=='%ws'", arg, argv[arg]);
				if(arg == 1)
				{
					if(FAILED(StringCchPrintfW(relativeAssetDir, MAX_PATH, 
					                           L"%ws", argv[arg])))
					{
						KLOG(ERROR, "Failed to extract relative asset path!");
						return RETURN_CODE_FAILURE;
					}
				}
			}
			LocalFree(argv);
		}
		else
		{
			KLOG(ERROR, "CommandLineToArgvW failed!");
		}
	}
	if(!jobQueueInit(&g_jobQueue))
	{
		KLOG(ERROR, "Failed to initialize job queue!");
		return RETURN_CODE_FAILURE;
	}
	// Spawn worker threads for g_jobQueue //
	W32ThreadInfo threadInfos[7];
	for(u32 threadIndex = 0; 
		threadIndex < sizeof(threadInfos)/sizeof(threadInfos[0]); 
		threadIndex++)
	{
		threadInfos[threadIndex].index    = threadIndex;
		threadInfos[threadIndex].jobQueue = &g_jobQueue;
		DWORD threadId;
		const HANDLE hThread = 
			CreateThread(nullptr, 0, w32WorkThread, 
			             &threadInfos[threadIndex], 0, &threadId);
		if(!hThread)
		{
			KLOG(ERROR, "Failed to create thread! getlasterror=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
		if(!CloseHandle(hThread))
		{
			KLOG(ERROR, "Failed to close thread handle! getlasterror=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
	}
	// Obtain and save a global copy of the app data folder path //
	//	Source: https://stackoverflow.com/a/2899042
	{
		TCHAR szPath[MAX_PATH];
		{
			const HRESULT result = 
				SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 
				                SHGFP_TYPE_CURRENT, szPath);
			if(result != S_OK)
			{
				KLOG(ERROR, "Failed to get APPDATA path! result=%ld", result);
				return RETURN_CODE_FAILURE;
			}
		}
		if(FAILED(StringCchPrintf(g_pathLocalAppData, MAX_PATH, TEXT("%s\\%s"), 
		                          szPath, APPLICATION_NAME)))
		{
			KLOG(ERROR, "Failed to build application APPDATA path!");
			return RETURN_CODE_FAILURE;
		}
		KLOG(INFO, "g_pathLocalAppData='%s'", g_pathLocalAppData);
		if(!CreateDirectory(g_pathLocalAppData, NULL))
		{
			const int errorCode = GetLastError();
			switch(errorCode)
			{
				case ERROR_ALREADY_EXISTS:
				{
					KLOG(INFO, "Application APPDATA path '%s' already exists.", 
					     g_pathLocalAppData);
				} break;
				default:
				{
					KLOG(ERROR, "Failed to create APPDATA path '%s'! "
					     "GetLastError=%i", 
					     g_pathLocalAppData, errorCode);
					return RETURN_CODE_FAILURE;
				} break;
			}
		}
	}
	// Obtain and save a global copy of the app temp data folder path //
	{
		TCHAR szPath[MAX_PATH];
		if(!GetTempPath(MAX_PATH, szPath))
		{
			KLOG(ERROR, "Failed to get OS temp path! GetLastError=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
		if(FAILED(StringCchPrintf(g_pathTemp, MAX_PATH, TEXT("%s%s"), szPath, 
		                          APPLICATION_NAME)))
		{
			KLOG(ERROR, "Failed to build application temp path!");
			return RETURN_CODE_FAILURE;
		}
		KLOG(INFO, "g_pathTemp='%s'", g_pathTemp);
		if(!CreateDirectory(g_pathTemp, NULL))
		{
			const int errorCode = GetLastError();
			switch(errorCode)
			{
				case ERROR_ALREADY_EXISTS:
				{
					KLOG(INFO, "Application temp path '%s' already exists.", 
					     g_pathTemp);
				} break;
				default:
				{
					KLOG(ERROR, "Failed to create app temp path '%s'! "
					     "GetLastError=%i", g_pathTemp, errorCode);
					return RETURN_CODE_FAILURE;
				} break;
			}
		}
	}
	// Locate where our exe file is on the OS.  This should also be the location
	//	where all our application's code modules live! //
	{
		if(!GetModuleFileNameA(NULL, g_pathToExe, MAX_PATH))
		{
			KLOG(ERROR, "Failed to get exe file path+name! GetLastError=%i", 
			     GetLastError());
			return RETURN_CODE_FAILURE;
		}
		char* lastBackslash = g_pathToExe;
		for(char* c = g_pathToExe; *c; c++)
		{
			if(*c == '\\')
			{
				lastBackslash = c;
			}
		}
		*(lastBackslash + 1) = 0;
	}
	// Locate what directory we should look in for assets //
	{
		if(FAILED(StringCchPrintf(g_pathToAssets, MAX_PATH, 
		                          TEXT("%s\\%wsassets"), 
		                          g_pathToExe, relativeAssetDir)))
		{
			KLOG(ERROR, "Failed to build g_pathToAssets!  g_pathToExe='%s'", 
			     g_pathToExe);
			return RETURN_CODE_FAILURE;
		}
	}
	///TODO: handle file paths longer than MAX_PATH in the future...
	TCHAR fullPathToGameDll    [MAX_PATH] = {};
	TCHAR fullPathToGameDllTemp[MAX_PATH] = {};
	// determine the FULL path to the game DLL file //
	{
		if(FAILED(StringCchPrintf(fullPathToGameDll, MAX_PATH, 
		                          TEXT("%s\\%s.dll"), g_pathToExe, 
		                          FILE_NAME_GAME_DLL)))
		{
			KLOG(ERROR, "Failed to build fullPathToGameDll!  "
			     "g_pathToExe='%s' FILE_NAME_GAME_DLL='%s'", 
			     g_pathToExe, FILE_NAME_GAME_DLL);
			return RETURN_CODE_FAILURE;
		}
		if(FAILED(StringCchPrintf(fullPathToGameDllTemp, MAX_PATH, 
		                          TEXT("%s\\%s_temp.dll"), g_pathTemp, 
		                          FILE_NAME_GAME_DLL)))
		{
			KLOG(ERROR, "Failed to build fullPathToGameDllTemp!  "
			     "g_pathTemp='%s' FILE_NAME_GAME_DLL='%s'", 
			     g_pathTemp, FILE_NAME_GAME_DLL);
			return RETURN_CODE_FAILURE;
		}
	}
	// allocate a dynamic memory arena for STB_IMAGE //
	{
		local_persist const SIZE_T STB_IMAGE_MEMORY_BYTES = kmath::megabytes(1);
		VOID*const stbImageMemory = VirtualAlloc(NULL, STB_IMAGE_MEMORY_BYTES, 
		                                         MEM_RESERVE | MEM_COMMIT, 
		                                         PAGE_READWRITE);
		if(!stbImageMemory)
		{
			KLOG(ERROR, "Failed to VirtualAlloc %i bytes for STB_IMAGE! "
			     "GetLastError=%i", 
			     STB_IMAGE_MEMORY_BYTES, GetLastError());
			return RETURN_CODE_FAILURE;
		}
		g_genAllocStbImage = kgaInit(stbImageMemory, STB_IMAGE_MEMORY_BYTES);
	}
	// allocate a dynamic memory arena for ImGui //
	{
		local_persist const SIZE_T IMGUI_MEMORY_BYTES = kmath::megabytes(1);
		VOID*const imguiMemory = VirtualAlloc(NULL, IMGUI_MEMORY_BYTES, 
		                                      MEM_RESERVE | MEM_COMMIT, 
		                                      PAGE_READWRITE);
		if(!imguiMemory)
		{
			KLOG(ERROR, "Failed to VirtualAlloc %i bytes for ImGui! "
			     "GetLastError=%i", 
			     IMGUI_MEMORY_BYTES, GetLastError());
			return RETURN_CODE_FAILURE;
		}
		g_genAllocImgui = kgaInit(imguiMemory, IMGUI_MEMORY_BYTES);
	}
	// allocate a chunk of memory for loading OGG files //
	{
		g_oggVorbisAlloc.alloc_buffer_length_in_bytes =
			kmath::safeTruncateI32(kmath::megabytes(1));
		g_oggVorbisAlloc.alloc_buffer = reinterpret_cast<char*>(
			VirtualAlloc(NULL, g_oggVorbisAlloc.alloc_buffer_length_in_bytes, 
			             MEM_RESERVE | MEM_COMMIT, 
			             PAGE_READWRITE) );
		if(!g_oggVorbisAlloc.alloc_buffer)
		{
			KLOG(ERROR, "Failed to VirtualAlloc %i bytes for OGG decoding! "
			     "GetLastError=%i", 
			     g_oggVorbisAlloc.alloc_buffer_length_in_bytes, GetLastError());
			return RETURN_CODE_FAILURE;
		}
	}
	GameCode game = w32LoadGameCode(fullPathToGameDll, 
	                                fullPathToGameDllTemp);
	kassert(game.isValid);
	if(!QueryPerformanceFrequency(&g_perfCounterHz))
	{
		KLOG(ERROR, "Failed to query for performance frequency! GetLastError=%i", 
		     GetLastError());
		return RETURN_CODE_FAILURE;
	}
#if 0
	w32ResizeDibSection(g_backBuffer, 1280, 720);
#endif
#if INTERNAL_BUILD
	g_displayCursor = true;
#endif
	g_cursorArrow          = LoadCursorA(NULL, IDC_ARROW);
	g_cursorSizeHorizontal = LoadCursorA(NULL, IDC_SIZEWE);
	g_cursorSizeVertical   = LoadCursorA(NULL, IDC_SIZENS);
	g_cursorSizeNeSw       = LoadCursorA(NULL, IDC_SIZENESW);
	g_cursorSizeNwSe       = LoadCursorA(NULL, IDC_SIZENWSE);
	const WNDCLASS wndClass = {
		.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		.lpfnWndProc   = w32MainWindowCallback,
		.hInstance     = hInstance,
		.hIcon         = LoadIcon(hInstance, TEXT("kml-application-icon")),
		.hCursor       = g_cursorArrow,
		.lpszClassName = "KmlWindowClass" };
	const ATOM atomWindowClass = RegisterClassA(&wndClass);
	if(atomWindowClass == 0)
	{
		KLOG(ERROR, "Failed to register WNDCLASS! GetLastError=%i", 
		     GetLastError());
		return RETURN_CODE_FAILURE;
	}
	g_mainWindow = CreateWindowExA(
		0,
		wndClass.lpszClassName,
		APPLICATION_NAME,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL );
	if(!g_mainWindow)
	{
		KLOG(ERROR, "Failed to create window! GetLastError=%i", GetLastError());
		return RETURN_CODE_FAILURE;
	}
	w32LoadDInput(hInstance);
	w32LoadXInput();
	w32KrbOglInitialize(g_mainWindow);
	w32KrbOglSetVSyncPreference(true);
	///TODO: update the monitorRefreshHz and dependent variable realtime when 
	///      the window gets moved around to another monitor.
	u32 monitorRefreshHz = w32QueryNearestMonitorRefreshRate(g_mainWindow);
	f32 targetSecondsElapsedPerFrame = 1.f / monitorRefreshHz;
	GameKeyboard gameKeyboardA = {};
	GameKeyboard gameKeyboardB = {};
	GameKeyboard* gameKeyboardCurrentFrame  = &gameKeyboardA;
	GameKeyboard* gameKeyboardPreviousFrame = &gameKeyboardB;
#if INTERNAL_BUILD
	// ensure that the size of the keyboard's vKeys array matches the size of
	//	the anonymous struct which defines the names of all the game keys //
	kassert( static_cast<size_t>(&gameKeyboardA.DUMMY_LAST_BUTTON_STATE - 
	                             &gameKeyboardA.vKeys[0]) ==
	         CARRAY_COUNT(gameKeyboardA.vKeys) );
	// ensure that the size of the gamepad's axis array matches the size of
	//	the anonymous struct which defines the names of all the axes //
	kassert( static_cast<size_t>(&g_gamePadArrayA[0].DUMMY_LAST_AXIS - 
	                             &g_gamePadArrayA[0].axes[0]) ==
	         CARRAY_COUNT(g_gamePadArrayA[0].axes) );
	// ensure that the size of the gamepad's button array matches the size of
	//	the anonymous struct which defines the names of all the buttons //
	kassert( static_cast<size_t>(&g_gamePadArrayA[0].DUMMY_LAST_BUTTON_STATE - 
	                             &g_gamePadArrayA[0].buttons[0]) ==
	         CARRAY_COUNT(g_gamePadArrayA[0].buttons) );
#endif// INTERNAL_BUILD
	local_persist const u8 SOUND_CHANNELS = 2;
	local_persist const u32 SOUND_SAMPLE_HZ = 44100;
	local_persist const u32 SOUND_BYTES_PER_SAMPLE = sizeof(i16)*SOUND_CHANNELS;
	local_persist const u32 SOUND_BUFFER_BYTES = 
		SOUND_SAMPLE_HZ/2 * SOUND_BYTES_PER_SAMPLE;
	DWORD cursorWritePrev;
	VOID*const gameSoundMemory = VirtualAlloc(NULL, SOUND_BUFFER_BYTES, 
	                                          MEM_RESERVE | MEM_COMMIT, 
	                                          PAGE_READWRITE);
	if(!gameSoundMemory)
	{
		KLOG(ERROR, "Failed to VirtualAlloc %i bytes for sound memory! "
		     "GetLastError=%i", SOUND_BUFFER_BYTES, GetLastError());
		return RETURN_CODE_FAILURE;
	}
	// Initialize ImGui~~~ //
	{
		ImGui::SetAllocatorFunctions(platformImguiAlloc, platformImguiFree, 
		                             g_genAllocImgui);
		ImGui::CreateContext();
		if(!ImGui_ImplOpenGL2_Init())
		{
			KLOG(ERROR, "ImGui_ImplOpenGL2_Init failure!");
			return RETURN_CODE_FAILURE;
		}
		if(!ImGui_ImplWin32_Init(g_mainWindow))
		{
			KLOG(ERROR, "ImGui_ImplWin32_Init failure!");
			return RETURN_CODE_FAILURE;
		}
	}
	GameMemory gameMemory = {};
#if INTERNAL_BUILD
	const LPVOID baseAddress = reinterpret_cast<LPVOID>(kmath::terabytes(1));
#else
	const LPVOID baseAddress = 0;
#endif
	gameMemory.permanentMemoryBytes = kmath::megabytes(16);
	gameMemory.transientMemoryBytes = kmath::megabytes(128);
	const u64 totalGameMemoryBytes = 
		gameMemory.permanentMemoryBytes + gameMemory.transientMemoryBytes;
	gameMemory.permanentMemory = VirtualAlloc(baseAddress, 
	                                          totalGameMemoryBytes, 
	                                          MEM_RESERVE | MEM_COMMIT, 
	                                          PAGE_READWRITE);
	if(!gameMemory.permanentMemory)
	{
		KLOG(ERROR, "Failed to VirtualAlloc %i bytes for game memory! "
		     "GetLastError=%i", totalGameMemoryBytes, GetLastError());
		return RETURN_CODE_FAILURE;
	}
	gameMemory.transientMemory = 
		reinterpret_cast<u8*>(gameMemory.permanentMemory) + 
		gameMemory.permanentMemoryBytes;
	gameMemory.kpl.postJob                = platformPostJob;
	gameMemory.kpl.jobDone                = platformJobDone;
	gameMemory.kpl.log                    = platformLog;
	gameMemory.kpl.decodeZ85Png           = platformDecodeZ85Png;
	gameMemory.kpl.decodeZ85Wav           = platformDecodeZ85Wav;
	gameMemory.kpl.loadWav                = platformLoadWav;
	gameMemory.kpl.loadOgg                = platformLoadOgg;
	gameMemory.kpl.loadPng                = platformLoadPng;
	gameMemory.kpl.loadFlipbookMeta       = platformLoadFlipbookMeta;
	gameMemory.kpl.getAssetWriteTime      = platformGetAssetWriteTime;
	gameMemory.kpl.isAssetChanged         = platformIsAssetChanged;
	gameMemory.kpl.isAssetAvailable       = platformIsAssetAvailable;
	gameMemory.kpl.isFullscreen           = platformIsFullscreen;
	gameMemory.kpl.setFullscreen          = platformSetFullscreen;
	gameMemory.kpl.getGamePadActiveButton = w32GetGamePadActiveButton;
	gameMemory.kpl.getGamePadActiveAxis   = w32GetGamePadActiveAxis;
	gameMemory.kpl.getGamePadProductName  = w32GetGamePadProductName;
	gameMemory.kpl.getGamePadProductGuid  = w32GetGamePadProductGuid;
#if INTERNAL_BUILD
	gameMemory.kpl.readEntireFile    = platformReadEntireFile;
	gameMemory.kpl.freeFileMemory    = platformFreeFileMemory;
	gameMemory.kpl.writeEntireFile   = platformWriteEntireFile;
#endif// INTERNAL_BUILD
	gameMemory.krb                 = {};
	gameMemory.imguiContext        = ImGui::GetCurrentContext();
	gameMemory.platformImguiAlloc  = platformImguiAlloc;
	gameMemory.platformImguiFree   = platformImguiFree;
	gameMemory.imguiAllocUserData  = g_genAllocImgui;
	game.onReloadCode(gameMemory);
	game.initialize(gameMemory);
	w32InitDSound(g_mainWindow, SOUND_SAMPLE_HZ, SOUND_BUFFER_BYTES, 
	              SOUND_CHANNELS, cursorWritePrev);
	const HDC hdc = GetDC(g_mainWindow);
	if(!hdc)
	{
		KLOG(ERROR, "Failed to get main window device context!");
		return RETURN_CODE_FAILURE;
	}
	local_persist const UINT DESIRED_OS_TIMER_GRANULARITY_MS = 1;
	// Determine if the system is capable of our desired timer granularity //
	bool systemSupportsDesiredTimerGranularity = false;
	{
		TIMECAPS timingCapabilities;
		if(timeGetDevCaps(&timingCapabilities, sizeof(TIMECAPS)) == 
			MMSYSERR_NOERROR)
		{
			systemSupportsDesiredTimerGranularity = 
				(timingCapabilities.wPeriodMin <= 
				 DESIRED_OS_TIMER_GRANULARITY_MS);
		}
		else
		{
			KLOG(WARNING, "System doesn't support custom scheduler granularity."
			     " Main thread will not be put to sleep!");
		}
	}
	// set scheduler granularity using timeBeginPeriod //
	const bool sleepIsGranular = systemSupportsDesiredTimerGranularity &&
		timeBeginPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) == TIMERR_NOERROR;
	if(!sleepIsGranular)
	{
		KLOG(WARNING, "System supports custom scheduler granularity, but "
		     "setting this value has failed! Main thread will not be "
		     "put to sleep!");
	}
	defer({
		if (sleepIsGranular && 
			timeEndPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) != TIMERR_NOERROR)
		{
			KLOG(ERROR, "Failed to timeEndPeriod!");
		}
	});
	// Set the process to a higher priority to minimize the chance of another 
	//	process keeping us asleep hopefully. //
	if(sleepIsGranular)
	{
		const HANDLE hProcess = GetCurrentProcess();
		if(!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS))
		{
			KLOG(ERROR, "Failed to set the process priority class to "
			     "HIGH_PRIORITY_CLASS! GetLastError=%i", GetLastError());
			return RETURN_CODE_FAILURE;
		}
	}
	// main window loop //
	{
		g_running = true;
		LARGE_INTEGER perfCountFrameLimiterPrev = w32QueryPerformanceCounter();
		LARGE_INTEGER perfCountPrev = w32QueryPerformanceCounter();
		u64 clockCyclesPrev = __rdtsc();
		while(g_running)
		{
			// dynamically hot-reload game code!!! //
			{
				const FILETIME gameCodeLastWriteTime = 
					w32GetLastWriteTime(fullPathToGameDll);
				if( gameCodeLastWriteTime.dwHighDateTime == 0 &&
				    gameCodeLastWriteTime.dwLowDateTime  == 0 )
				// Immediately invalidate the game code if we fail to get the 
				//	last write time of the dll, since it means the dll is 
				//	probably in the process of getting recompiled!
				{
					game.isValid = false;
				}
				if( CompareFileTime(&gameCodeLastWriteTime, 
				                    &game.dllLastWriteTime) ||
				    !game.isValid )
				{
					w32UnloadGameCode(game);
					game = w32LoadGameCode(fullPathToGameDll, 
					                       fullPathToGameDllTemp);
					if(game.isValid)
					{
						KLOG(INFO, "Game code hot-reload complete!");
						game.onReloadCode(gameMemory);
					}
				}
			}
			MSG windowMessage;
			while(PeekMessageA(&windowMessage, NULL, 0, 0, PM_REMOVE))
			{
				switch(windowMessage.message)
				{
					case WM_QUIT:
					{
						g_running = false;
					} break;
					default:
					{
						TranslateMessage(&windowMessage);
						DispatchMessageA(&windowMessage);
					} break;
				}
			}
			if(g_deviceChangeDetected)
			{
				g_deviceChangeDetected = false;
				w32DInputEnumerateDevices();
			}
			// Process gameKeyboard by comparing the keys to the previous 
			//	frame's keys, because we cannot determine if a key has been 
			//	newly pressed this frame just from the windows message loop data
			if(gameKeyboardCurrentFrame == &gameKeyboardA)
			{
				gameKeyboardCurrentFrame  = &gameKeyboardB;
				gameKeyboardPreviousFrame = &gameKeyboardA;
			}
			else
			{
				gameKeyboardCurrentFrame  = &gameKeyboardA;
				gameKeyboardPreviousFrame = &gameKeyboardB;
			}
			w32GetKeyboardKeyStates(gameKeyboardCurrentFrame, 
			                        gameKeyboardPreviousFrame);
			// swap game pad arrays & update the current frame //
			if(g_gamePadArrayCurrentFrame == g_gamePadArrayA)
			{
				g_gamePadArrayPreviousFrame = g_gamePadArrayA;
				g_gamePadArrayCurrentFrame  = g_gamePadArrayB;
			}
			else
			{
				g_gamePadArrayPreviousFrame = g_gamePadArrayB;
				g_gamePadArrayCurrentFrame  = g_gamePadArrayA;
			}
			/* read game pads from DirectInput & XInput */
			w32DInputGetGamePadStates(g_gamePadArrayCurrentFrame  + 
			                              XUSER_MAX_COUNT, 
			                          g_gamePadArrayPreviousFrame + 
			                              XUSER_MAX_COUNT);
			w32XInputGetGamePadStates(g_gamePadArrayCurrentFrame,
			                          g_gamePadArrayPreviousFrame);
			ImGui_ImplOpenGL2_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			const W32Dimension2d windowDims = 
				w32GetWindowDimensions(g_mainWindow);
			if(!game.isValid)
			// display a "loading" message while we wait for cl.exe to 
			//	relinquish control of the game binary //
			{
				const ImVec2 displayCenter(
					windowDims.width*0.5f, windowDims.height*0.5f);
				ImGui::SetNextWindowPos(displayCenter, ImGuiCond_Always, 
				                        ImVec2(0.5f, 0.5f));
				ImGui::Begin("Reloading Game Code", nullptr, 
				             ImGuiWindowFlags_NoTitleBar |
				             ImGuiWindowFlags_NoResize |
				             ImGuiWindowFlags_NoMove |
				             ImGuiWindowFlags_NoScrollbar |
				             ImGuiWindowFlags_NoScrollWithMouse |
				             ImGuiWindowFlags_AlwaysAutoResize |
				             ImGuiWindowFlags_NoSavedSettings |
				             ImGuiWindowFlags_NoMouseInputs );
				ImGui::Text("Loading Game Code...%c", 
				            "|/-\\"[(int)(ImGui::GetTime() / 0.05f) & 3]);
				ImGui::End();
			}
			const f32 deltaSeconds = 
				min(MAX_GAME_DELTA_SECONDS, targetSecondsElapsedPerFrame);
			if(!game.updateAndDraw(deltaSeconds,
			                       {windowDims.width, windowDims.height}, 
			                       *gameKeyboardCurrentFrame,
			                       g_gamePadArrayCurrentFrame, 
			                       CARRAY_COUNT(g_gamePadArrayA), 
			                       g_isFocused))
			{
				g_running = false;
			}
			w32WriteDSoundAudio(SOUND_BUFFER_BYTES, SOUND_SAMPLE_HZ, 
			                    SOUND_CHANNELS, gameSoundMemory, 
			                    cursorWritePrev, game);
#if 0
			// set XInput state //
			for(u8 ci = 0; ci < numGamePads; ci++)
			{
				XINPUT_VIBRATION vibration;
				vibration.wLeftMotorSpeed = static_cast<WORD>(0xFFFF *
					gamePadArrayCurrentFrame[ci].normalizedMotorSpeedLeft);
				vibration.wRightMotorSpeed = static_cast<WORD>(0xFFFF *
					gamePadArrayCurrentFrame[ci].normalizedMotorSpeedRight);
				if(XInputSetState(ci, &vibration) != ERROR_SUCCESS)
				{
					///TODO: error & controller not connected return values
				}
			}
#endif // 0
			// update window graphics //
			ImGui::Render();
			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
			if(!SwapBuffers(hdc))
			{
				KLOG(ERROR, "Failed to SwapBuffers! GetLastError=%i", 
				     GetLastError());
			}
			// enforce targetSecondsElapsedPerFrame //
			// we still have to Sleep/wait when VSync is on if SwapBuffers
			//      completes too early!!! (like when the double-buffer is not
			//      yet filled up at beginning of execution) //
			///TODO: maybe just don't sleep/wait the first frame at all because
			///      the windows message queue needs to be unclogged and game
			///      state memory needs to be initialized.
			{
				// It is possible for windows to sleep us for longer than we 
				//	would want it to, so we will ask the OS to wake us up a 
				//	little bit earlier than desired.  Then we will spin the CPU
				//	until the actual target FPS is achieved.
				// THREAD_WAKE_SLACK_SECONDS is how early we should wake up by.
				local_persist const f32 THREAD_WAKE_SLACK_SECONDS = 0.001f;
				f32 awakeSeconds = 
					w32ElapsedSeconds(perfCountFrameLimiterPrev, 
					                  w32QueryPerformanceCounter());
				const f32 targetAwakeSeconds = 
					targetSecondsElapsedPerFrame - THREAD_WAKE_SLACK_SECONDS;
				while(awakeSeconds < targetSecondsElapsedPerFrame)
				{
					if(sleepIsGranular && awakeSeconds < targetAwakeSeconds)
					{
						const DWORD sleepMilliseconds = 
							static_cast<DWORD>(1000 * 
							(targetAwakeSeconds - awakeSeconds));
						Sleep(sleepMilliseconds);
					}
					awakeSeconds = 
						w32ElapsedSeconds(perfCountFrameLimiterPrev, 
						                  w32QueryPerformanceCounter());
				}
				perfCountFrameLimiterPrev = w32QueryPerformanceCounter();
			}
			// measure main loop performance //
			{
				const LARGE_INTEGER perfCount = w32QueryPerformanceCounter();
				const f32 elapsedSeconds = 
					w32ElapsedSeconds(perfCountPrev, perfCount);
				const u64 clockCycles = __rdtsc();
				const u64 clockCycleDiff = clockCycles - clockCyclesPrev;
#if 0
				const LONGLONG elapsedMicroseconds = 
					(perfCountDiff*1000000) / g_perfCounterHz.QuadPart;
#endif
#if SLOW_BUILD && 0
				// send performance measurement to debugger as a string //
				KLOG(INFO, "%.2f ms/f | %.2f Mc/f", 
				     elapsedSeconds*1000, clockCycleDiff/1000000.f);
#endif
				perfCountPrev   = perfCount;
				clockCyclesPrev = clockCycles;
			}
		}
	}
	kassert(kgaUsedBytes(g_genAllocStbImage) == 0);
	KLOG(INFO, "END! :)");
	return RETURN_CODE_SUCCESS;
}
#include "win32-dsound.cpp"
#include "win32-xinput.cpp"
#include "win32-directinput.cpp"
#include "win32-krb-opengl.cpp"
#include "win32-jobQueue.cpp"
#include "krb-opengl.cpp"
#include "z85.cpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ASSERT(x) kassert(x)
internal void* stbiMalloc(size_t allocationByteCount)
{
	EnterCriticalSection(&g_stbiAllocationCsLock);
	defer(LeaveCriticalSection(&g_stbiAllocationCsLock));
	return kgaAlloc(g_genAllocStbImage, allocationByteCount);
}
internal void* stbiRealloc(void* allocatedAddress, 
                           size_t newAllocationByteCount)
{
	EnterCriticalSection(&g_stbiAllocationCsLock);
	defer(LeaveCriticalSection(&g_stbiAllocationCsLock));
	return kgaRealloc(g_genAllocStbImage, 
	                  allocatedAddress, newAllocationByteCount);
}
internal void stbiFree(void* allocatedAddress)
{
	EnterCriticalSection(&g_stbiAllocationCsLock);
	kgaFree(g_genAllocStbImage, allocatedAddress);
	LeaveCriticalSection(&g_stbiAllocationCsLock);
}
#define STBI_MALLOC(sz)       stbiMalloc(sz)
#define STBI_REALLOC(p,newsz) stbiRealloc(p,newsz)
#define STBI_FREE(p)          stbiFree(p)
#include "stb/stb_image.h"
#include "kGeneralAllocator.cpp"
#pragma warning( push )
// warning C4127: conditional expression is constant
#pragma warning( disable : 4127 )
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_impl_opengl2.cpp"
#include "imgui/imgui_impl_win32.cpp"
#include "imgui/imgui_widgets.cpp"
#include "imgui/imgui.cpp"
#pragma warning( pop )
#pragma warning( push )
#pragma warning( disable : 4244 )
#pragma warning( disable : 4245 )
#pragma warning( disable : 4369 )
#pragma warning( disable : 4456 )
#pragma warning( disable : 4457 )
#pragma warning( disable : 4701 )
#include "stb/stb_vorbis.c"
#pragma warning( pop )