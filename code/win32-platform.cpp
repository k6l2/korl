#include "win32-platform.h"
#include "win32-main.h"
#include "win32-xinput.h"
#include "win32-directinput.h"
#include <strsafe.h>
#include "z85.h"
#include "stb/stb_image.h"
global_variable WINDOWPLACEMENT g_lastKnownWindowedPlacement;
internal PLATFORM_IS_FULLSCREEN(w32PlatformIsFullscreen)
{
	const DWORD windowStyle = GetWindowLong(g_mainWindow, GWL_STYLE);
	if(windowStyle == 0)
	{
		KLOG(ERROR, "Failed to get window style!");
		return false;
	}
	return !(windowStyle & WS_OVERLAPPEDWINDOW);
}
internal PLATFORM_SET_FULLSCREEN(w32PlatformSetFullscreen)
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
internal PLATFORM_POST_JOB(w32PlatformPostJob)
{
	return jobQueuePostJob(&g_jobQueue, function, data);
}
internal PLATFORM_JOB_DONE(w32PlatformJobDone)
{
	return jobQueueJobIsDone(&g_jobQueue, ticket);
}
internal PLATFORM_JOB_VALID(w32PlatformJobValid)
{
	return jobQueueTicketIsValid(&g_jobQueue, ticket);
}
internal RawImage w32DecodePng(const void* fileMemory, size_t fileBytes,
                               KgaHandle pixelDataAllocator)
{
	i32 imgW, imgH, imgNumByteChannels;
	u8*const img = 
		stbi_load_from_memory(reinterpret_cast<const u8*>(fileMemory), 
		                      kmath::safeTruncateI32(fileBytes), &imgW, &imgH, 
		                      &imgNumByteChannels, 4);
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
		.sizeX     = kmath::safeTruncateU32(imgW),
		.sizeY     = kmath::safeTruncateU32(imgH), 
		.pixelData = pixelData };
}
internal PLATFORM_LOAD_PNG(w32PlatformLoadPng)
{
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, fileName);
	const i32 rawFileBytes = w32PlatformGetFileByteSize(szFileFullPath);
	if(rawFileBytes < 0)
	{
		KLOG(ERROR, "Failed to get file byte size '%s'!", szFileFullPath);
		return {};
	}
	EnterCriticalSection(&g_csLockAllocatorRawFiles);
	void* rawFileMemory = kgaAlloc(g_hKgaRawFiles, rawFileBytes);
	LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	if(!rawFileMemory)
	{
		KLOG(ERROR, "Failed to allocate %i bytes for raw file!", rawFileBytes);
		return {};
	}
	defer({
		EnterCriticalSection(&g_csLockAllocatorRawFiles);
		kgaFree(g_hKgaRawFiles, rawFileMemory);
		LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	});
	const bool fileReadResult = 
		w32PlatformReadEntireFile(szFileFullPath, rawFileMemory, rawFileBytes);
	if(!fileReadResult)
	{
		KLOG(ERROR, "Failed to read entire file '%s'!", szFileFullPath);
		return {};
	}
	return w32DecodePng(rawFileMemory, rawFileBytes, pixelDataAllocator);
}
internal PLATFORM_LOAD_OGG(w32PlatformLoadOgg)
{
	// Load the entire OGG file into memory //
	RawSound result = {};
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, fileName);
	const i32 rawFileBytes = w32PlatformGetFileByteSize(szFileFullPath);
	if(rawFileBytes < 0)
	{
		KLOG(ERROR, "Failed to get file byte size '%s'!", szFileFullPath);
		return result;
	}
	EnterCriticalSection(&g_csLockAllocatorRawFiles);
	void* rawFileMemory = kgaAlloc(g_hKgaRawFiles, rawFileBytes);
	LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	if(!rawFileMemory)
	{
		KLOG(ERROR, "Failed to allocate %i bytes for raw file!", rawFileBytes);
		return result;
	}
	defer({
		EnterCriticalSection(&g_csLockAllocatorRawFiles);
		kgaFree(g_hKgaRawFiles, rawFileMemory);
		LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	});
	const bool fileReadResult = 
		w32PlatformReadEntireFile(szFileFullPath, rawFileMemory, rawFileBytes);
	if(!fileReadResult)
	{
		KLOG(ERROR, "Failed to read entire file '%s'!", szFileFullPath);
		return result;
	}
	// Decode the OGG file into a RawSound //
	int vorbisError;
	EnterCriticalSection(&g_vorbisAllocationCsLock);
	stb_vorbis*const vorbis = 
		stb_vorbis_open_memory(reinterpret_cast<u8*>(rawFileMemory), 
		                       rawFileBytes, &vorbisError, &g_oggVorbisAlloc);
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
internal RawSound w32DecodeWav(const void* fileMemory, size_t fileBytes, 
                               KgaHandle sampleDataAllocator, 
                               const TCHAR* szFileFullPath)
{
	// Now that we have the entire file in memory, we can verify if the file is
	//	valid and extract necessary meta data simultaneously! //
	const u8* currByte = reinterpret_cast<const u8*>(fileMemory);
	union U32Chars
	{
		u32 uInt;
		char chars[4];
	} u32Chars;
	u32Chars.chars[0] = 'R';
	u32Chars.chars[1] = 'I';
	u32Chars.chars[2] = 'F';
	u32Chars.chars[3] = 'F';
	if(u32Chars.uInt != *reinterpret_cast<const u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.ChunkId==0x%x invalid for '%s'!", 
		     reinterpret_cast<const u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	const u32 riffChunkSize = *reinterpret_cast<const u32*>(currByte);
	currByte += 4;
	u32Chars.chars[0] = 'W';
	u32Chars.chars[1] = 'A';
	u32Chars.chars[2] = 'V';
	u32Chars.chars[3] = 'E';
	if(u32Chars.uInt != *reinterpret_cast<const u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.Format==0x%x invalid/unsupported for '%s'!", 
		     *reinterpret_cast<const u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	u32Chars.chars[0] = 'f';
	u32Chars.chars[1] = 'm';
	u32Chars.chars[2] = 't';
	u32Chars.chars[3] = ' ';
	if(u32Chars.uInt != *reinterpret_cast<const u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.fmt.id==0x%x invalid for '%s'!", 
		     *reinterpret_cast<const u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	const u32 riffFmtChunkSize = *reinterpret_cast<const u32*>(currByte);
	currByte += 4;
	if(riffFmtChunkSize != 16)
	{
		KLOG(ERROR, "RIFF.fmt.chunkSize==%i invalid/unsupported for '%s'!", 
		     riffFmtChunkSize, szFileFullPath);
		return {};
	}
	const u16 riffFmtAudioFormat = *reinterpret_cast<const u16*>(currByte);
	currByte += 2;
	if(riffFmtAudioFormat != 1)
	{
		KLOG(ERROR, "RIFF.fmt.audioFormat==%i invalid/unsupported for '%s'!",
		     riffFmtAudioFormat, szFileFullPath);
		return {};
	}
	const u16 riffFmtNumChannels = *reinterpret_cast<const u16*>(currByte);
	currByte += 2;
	if(riffFmtNumChannels > 255 || riffFmtNumChannels == 0)
	{
		KLOG(ERROR, "RIFF.fmt.numChannels==%i invalid/unsupported for '%s'!",
		     riffFmtNumChannels, szFileFullPath);
		return {};
	}
	const u32 riffFmtSampleRate = *reinterpret_cast<const u32*>(currByte);
	currByte += 4;
	if(riffFmtSampleRate != 44100)
	{
		KLOG(ERROR, "RIFF.fmt.riffFmtSampleRate==%i invalid/unsupported for "
		     "'%s'!", riffFmtSampleRate, szFileFullPath);
		return {};
	}
	const u32 riffFmtByteRate = *reinterpret_cast<const u32*>(currByte);
	currByte += 4;
	const u16 riffFmtBlockAlign = *reinterpret_cast<const u16*>(currByte);
	currByte += 2;
	const u16 riffFmtBitsPerSample = *reinterpret_cast<const u16*>(currByte);
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
	if(u32Chars.uInt != *reinterpret_cast<const u32*>(currByte))
	{
		KLOG(ERROR, "RIFF.data.id==0x%x invalid for '%s'!", 
		     *reinterpret_cast<const u32*>(currByte), szFileFullPath);
		return {};
	}
	currByte += 4;
	const u32 riffDataSize = *reinterpret_cast<const u32*>(currByte);
	currByte += 4;
	// Now we can extract the actual sound data from the WAV file //
	if(currByte + riffDataSize > 
		reinterpret_cast<const u8*>(fileMemory) + fileBytes)
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
internal PLATFORM_LOAD_WAV(w32PlatformLoadWav)
{
	// Load the entire WAV file into memory //
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\%s"), 
	                 g_pathToAssets, fileName);
	const i32 rawFileBytes = w32PlatformGetFileByteSize(szFileFullPath);
	if(rawFileBytes < 0)
	{
		KLOG(ERROR, "Failed to get file byte size '%s'!", szFileFullPath);
		return {};
	}
	EnterCriticalSection(&g_csLockAllocatorRawFiles);
	void* rawFileMemory = kgaAlloc(g_hKgaRawFiles, rawFileBytes);
	LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	if(!rawFileMemory)
	{
		KLOG(ERROR, "Failed to allocate %i bytes for raw file!", rawFileBytes);
		return {};
	}
	defer({
		EnterCriticalSection(&g_csLockAllocatorRawFiles);
		kgaFree(g_hKgaRawFiles, rawFileMemory);
		LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	});
	const bool fileReadResult = 
		w32PlatformReadEntireFile(szFileFullPath, rawFileMemory, rawFileBytes);
	if(!fileReadResult)
	{
		KLOG(ERROR, "Failed to read entire file '%s'!", szFileFullPath);
		return {};
	}
	return w32DecodeWav(rawFileMemory, rawFileBytes, 
	                    sampleDataAllocator, szFileFullPath);
}
internal PLATFORM_IMGUI_ALLOC(w32PlatformImguiAlloc)
{
	/* imgui allocations don't have to be thread-safe because they should ONLY 
		proc on the main thread! @TODO: assert this is the main thread */
	return kgaAlloc(user_data, sz);
}
internal PLATFORM_IMGUI_FREE(w32PlatformImguiFree)
{
	/* imgui allocations don't have to be thread-safe because they should ONLY 
		proc on the main thread! @TODO: assert this is the main thread */
	kgaFree(user_data, ptr);
}
internal PLATFORM_DECODE_Z85_PNG(w32PlatformDecodeZ85Png)
{
	const u32 rawFileBytes = kmath::safeTruncateU32(
		z85::decodedFileSizeBytes(z85PngNumBytes));
	if(rawFileBytes == 0)
	{
		KLOG(ERROR, "Invalid z85 byte count (%i)!", z85PngNumBytes);
		return {};
	}
	EnterCriticalSection(&g_csLockAllocatorRawFiles);
	i8* rawFileMemory = reinterpret_cast<i8*>(
		kgaAlloc(g_hKgaRawFiles, rawFileBytes));
	LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	if(!rawFileMemory)
	{
		KLOG(ERROR, "Failed to allocate %i bytes for raw file!", rawFileBytes);
		return {};
	}
	defer({
		EnterCriticalSection(&g_csLockAllocatorRawFiles);
		kgaFree(g_hKgaRawFiles, rawFileMemory);
		LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	});
	if(!z85::decode(reinterpret_cast<const i8*>(z85PngData), rawFileMemory))
	{
		KLOG(ERROR, "z85::decode failure!");
		return {};
	}
	return w32DecodePng(rawFileMemory, rawFileBytes, pixelDataAllocator);
}
internal PLATFORM_DECODE_Z85_WAV(w32PlatformDecodeZ85Wav)
{
	const u32 rawFileBytes = kmath::safeTruncateU32(
		z85::decodedFileSizeBytes(z85WavNumBytes));
	if(rawFileBytes == 0)
	{
		KLOG(ERROR, "Invalid z85 byte count (%i)!", z85WavNumBytes);
		return {};
	}
	EnterCriticalSection(&g_csLockAllocatorRawFiles);
	i8* rawFileMemory = reinterpret_cast<i8*>(
		kgaAlloc(g_hKgaRawFiles, rawFileBytes));
	LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	if(!rawFileMemory)
	{
		KLOG(ERROR, "Failed to allocate %i bytes for raw file!", rawFileBytes);
		return {};
	}
	defer({
		EnterCriticalSection(&g_csLockAllocatorRawFiles);
		kgaFree(g_hKgaRawFiles, rawFileMemory);
		LeaveCriticalSection(&g_csLockAllocatorRawFiles);
	});
	if(!z85::decode(reinterpret_cast<const i8*>(z85WavData), rawFileMemory))
	{
		KLOG(ERROR, "z85::decode failure!");
		return {};
	}
	return w32DecodeWav(rawFileMemory, rawFileBytes, 
	                    sampleDataAllocator, TEXT("z85_data"));
}
internal PLATFORM_LOG(w32PlatformLog)
{
	SYSTEMTIME stLocalTime;
	GetLocalTime( &stLocalTime );
	TCHAR timeBuffer[80];
	StringCchPrintf(timeBuffer, CARRAY_SIZE(timeBuffer), 
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
		            kutil::fileName(sourceFileName), sourceFileLineNumber);
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
			          __FILE__, __LINE__);
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
internal PLATFORM_ASSERT(w32PlatformAssert)
{
	if(expression)
		return;
	if(IsDebuggerPresent())
	{
		DebugBreak();
	}
	else
		*(int*)0 = 0;
}
internal PLATFORM_GET_FILE_BYTE_SIZE(w32PlatformGetFileByteSize)
{
	i32 resultFileByteSize = 0;
	const HANDLE fileHandle = 
		CreateFileA(ansiFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, 
		            OPEN_EXISTING, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		KLOG(ERROR, "Failed to create file handle! GetLastError=%i",
		     GetLastError());
		return -1;
	}
	defer({
		if(!CloseHandle(fileHandle))
		{
			KLOG(ERROR, "Failed to close file handle! GetLastError=%i",
			     GetLastError());
			resultFileByteSize = -1;
		}
	});
	LARGE_INTEGER fileSize;
	if(!GetFileSizeEx(fileHandle, &fileSize))
	{
		KLOG(ERROR, "Failed to get file size! GetLastError=%i", GetLastError());
		return -1;
	}
	resultFileByteSize = kmath::safeTruncateI32(fileSize.QuadPart);
	if(resultFileByteSize != fileSize.QuadPart)
	{
		KLOG(ERROR, "File size exceeds size of resultFileByteSize!");
		return -1;
	}
	return resultFileByteSize;
}
internal PLATFORM_READ_ENTIRE_FILE(w32PlatformReadEntireFile)
{
	/* use a variable for the return value so we can update it in defer 
		statements if needed */
	bool deferredResult = true;
	const HANDLE fileHandle = 
		CreateFileA(ansiFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, 
		            OPEN_EXISTING, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		KLOG(ERROR, "Failed to create file handle! GetLastError=%i",
		     GetLastError());
		return false;
	}
	defer({
		if(!CloseHandle(fileHandle))
		{
			KLOG(ERROR, "Failed to close file handle! GetLastError=%i",
			     GetLastError());
			deferredResult = false;
		}
	});
	/* get the size of the file we're about to read in & ensure that o_data has 
		enough bytes to fit the entire file */
	u32 fileSize32;
	{
		LARGE_INTEGER fileSize;
		if(!GetFileSizeEx(fileHandle, &fileSize))
		{
			KLOG(ERROR, "Failed to get file size! GetLastError=%i",
			     GetLastError());
			return false;
		}
		fileSize32 = kmath::safeTruncateU32(fileSize.QuadPart);
		if(fileSize32 != fileSize.QuadPart)
		{
			KLOG(ERROR, "File size exceeds 32 bits!");
			return false;
		}
		if(dataBytes < fileSize32)
		{
			KLOG(ERROR, "dataBytes=%i < fileSize32=%i!", dataBytes, fileSize32);
			return false;
		}
	}
	DWORD numBytesRead;
	if(!ReadFile(fileHandle, o_data, fileSize32, &numBytesRead, nullptr))
	{
		KLOG(ERROR, "Failed to read file! GetLastError=%i", 
		     fileSize32, GetLastError());
		return false;
	}
	if(numBytesRead != fileSize32)
	{
		KLOG(ERROR, "Failed to read file! bytes read: %i / %i", 
		     numBytesRead, fileSize32);
		return false;
	}
	return deferredResult;
}
internal PLATFORM_WRITE_ENTIRE_FILE(w32PlatformWriteEntireFile)
{
	const HANDLE fileHandle = 
		CreateFileA(ansiFileName, GENERIC_WRITE, 0, nullptr, 
		            CREATE_ALWAYS, 
		            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
	if(fileHandle == INVALID_HANDLE_VALUE)
	{
		KLOG(ERROR, "Failed to create write file handle '%s'! GetLastError=%i", 
		     ansiFileName, GetLastError());
		return false;
	}
	bool result = true;
	defer({
		if(!CloseHandle(fileHandle))
		{
			KLOG(ERROR, "Failed to close file handle '%s'! GetLastError=%i", 
			     ansiFileName, GetLastError());
			result = false;
		}
	});
	DWORD bytesWritten;
	if(!WriteFile(fileHandle, data, dataBytes, &bytesWritten, nullptr))
	{
		KLOG(ERROR, "Failed to write to file '%s'! GetLastError=%i", 
		     ansiFileName, GetLastError());
		return false;
	}
	if(bytesWritten != dataBytes)
	{
		KLOG(ERROR, "Failed to completely write to file '%s'! "
		     "Bytes written: %i / %i", 
		     ansiFileName, bytesWritten, dataBytes);
		return false;
	}
	return result;
}
internal PLATFORM_GET_GAME_PAD_ACTIVE_BUTTON(w32PlatformGetGamePadActiveButton)
{
	kassert(gamePadIndex < CARRAY_SIZE(g_gamePadArrayA));
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
internal PLATFORM_GET_GAME_PAD_ACTIVE_AXIS(w32PlatformGetGamePadActiveAxis)
{
	kassert(gamePadIndex < CARRAY_SIZE(g_gamePadArrayA));
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
internal PLATFORM_GET_GAME_PAD_PRODUCT_NAME(w32PlatformGetGamePadProductName)
{
	kassert(gamePadIndex < CARRAY_SIZE(g_gamePadArrayA));
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
internal PLATFORM_GET_GAME_PAD_PRODUCT_GUID(w32PlatformGetGamePadProductGuid)
{
	kassert(gamePadIndex < CARRAY_SIZE(g_gamePadArrayA));
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
internal PLATFORM_GET_ASSET_WRITE_TIME(w32PlatformGetAssetWriteTime)
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
internal PLATFORM_IS_ASSET_CHANGED(w32PlatformIsAssetChanged)
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
internal PLATFORM_IS_ASSET_AVAILABLE(w32PlatformIsAssetAvailable)
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
union PlatformTimeStampUnion
{
	static_assert(sizeof(PlatformTimeStamp) <= sizeof(LARGE_INTEGER));
	LARGE_INTEGER largeInt;
	PlatformTimeStamp timeStamp;
};
internal PLATFORM_GET_TIMESTAMP(w32PlatformGetTimeStamp)
{
	PlatformTimeStampUnion platformTimeStampUnion;
	platformTimeStampUnion.largeInt = w32QueryPerformanceCounter();
	return platformTimeStampUnion.timeStamp;
}
internal PLATFORM_SECONDS_SINCE_TIMESTAMP(w32PlatformSecondsSinceTimeStamp)
{
	PlatformTimeStampUnion platformTimeStampUnionPrevious;
	platformTimeStampUnionPrevious.timeStamp = pts;
	PlatformTimeStampUnion platformTimeStampUnion;
	platformTimeStampUnion.largeInt = w32QueryPerformanceCounter();
	/* calculate the # of seconds between the previous timestamp and the current 
		timestamp */
	kassert(platformTimeStampUnion.largeInt.QuadPart >= 
	        platformTimeStampUnionPrevious.largeInt.QuadPart);
	const LONGLONG perfCountDiff = 
		platformTimeStampUnion.largeInt.QuadPart - 
		platformTimeStampUnionPrevious.largeInt.QuadPart;
	const f32 elapsedSeconds = 
		static_cast<f32>(perfCountDiff) / g_perfCounterHz.QuadPart;
	return elapsedSeconds;
}
internal PLATFORM_SLEEP_FROM_TIMESTAMP(w32PlatformSleepFromTimeStamp)
{
	const f32 elapsedSeconds = w32PlatformSecondsSinceTimeStamp(pts);
	/* conditionally sleep depending on the # of elapsed seconds */
	if(elapsedSeconds >= desiredDeltaSeconds)
		return;
	const DWORD sleepMilliseconds = 
		static_cast<DWORD>(1000 * (desiredDeltaSeconds - elapsedSeconds));
	Sleep(sleepMilliseconds);
}
internal PLATFORM_RESERVE_LOCK(w32PlatformReserveLock)
{
	KplLockHandle result = 0;
	for(size_t l = 0; l < CARRAY_SIZE(g_platformExposedLocks); l++)
	{
		if(!g_platformExposedLocks[l].allocated)
		{
			g_platformExposedLocks[l].allocated = true;
			return kmath::safeTruncateU8(l + 1);
		}
	}
	return result;
}
internal PLATFORM_LOCK(w32PlatformLock)
{
	kassert(hLock > 0);
	kassert(g_platformExposedLocks[hLock - 1].allocated);
	EnterCriticalSection(&g_platformExposedLocks[hLock - 1].csLock);
}
internal PLATFORM_UNLOCK(w32PlatformUnlock)
{
	kassert(hLock > 0);
	kassert(g_platformExposedLocks[hLock - 1].allocated);
	LeaveCriticalSection(&g_platformExposedLocks[hLock - 1].csLock);
}