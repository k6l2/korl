#include "win32-main.h"
#include "win32-dsound.h"
#include "win32-xinput.h"
#include "win32-krb-opengl.h"
#include "generalAllocator.h"
#include <cstdio>
#include <ctime>
#include <dbghelp.h>
#include <strsafe.h>
#include <ShlObj.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
                             HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include "imgui/imgui_impl_opengl2.h"
#include "z85.h"
#include "stb/stb_image.h"
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
global_variable bool g_running;
global_variable bool g_displayCursor;
global_variable HCURSOR g_cursor;
global_variable W32OffscreenBuffer g_backBuffer;
global_variable LARGE_INTEGER g_perfCounterHz;
global_variable KgaHandle g_genAllocStbImage;
global_variable KgaHandle g_genAllocImgui;
global_variable const size_t MAX_LOG_BUFFER_SIZE = 32768;
global_variable char g_logBeginning[MAX_LOG_BUFFER_SIZE];
global_variable char g_logCircularBuffer[MAX_LOG_BUFFER_SIZE] = {};
global_variable size_t g_logCurrentBeginningChar;
global_variable size_t g_logCurrentCircularBufferChar;
// This represents the total # of character sent to the circular buffer, 
//	including characters that are overwritten!
global_variable size_t g_logCircularBufferRunningTotal;
global_variable bool g_hasReceivedException;
// @assumption: once we have written a crash dump, there is never a need to 
//	write any more, let alone continue execution.
global_variable bool g_hasWrittenCrashDump;
///TODO: handle file paths longer than MAX_PATH in the future...
global_variable TCHAR g_pathToExe[MAX_PATH];
global_variable TCHAR g_pathTemp[MAX_PATH];
global_variable TCHAR g_pathLocalAppData[MAX_PATH];
internal PLATFORM_LOAD_WAV(platformLoadWav)
{
	RawSound result = {};
	char szFileFullPath[MAX_PATH];
	StringCchPrintfA(szFileFullPath, MAX_PATH, TEXT("%s\\..\\%s"), 
	                 g_pathToExe, fileName);
	PlatformDebugReadFileResult file = platformReadEntireFile(szFileFullPath);
	if(!file.data)
	{
		KLOG_ERROR("Failed to read entire file '%s'!", szFileFullPath);
		return result;
	}
	defer(platformFreeFileMemory(file.data));
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
		KLOG_ERROR("RIFF.ChunkId==0x%x invalid for '%s'!", 
		           reinterpret_cast<u32*>(currByte), szFileFullPath);
		return result;
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
		KLOG_ERROR("RIFF.Format==0x%x invalid/unsupported for '%s'!", 
		           *reinterpret_cast<u32*>(currByte), szFileFullPath);
		return result;
	}
	currByte += 4;
	u32Chars.chars[0] = 'f';
	u32Chars.chars[1] = 'm';
	u32Chars.chars[2] = 't';
	u32Chars.chars[3] = ' ';
	if(u32Chars.uInt != *reinterpret_cast<u32*>(currByte))
	{
		KLOG_ERROR("RIFF.fmt.id==0x%x invalid for '%s'!", 
		           *reinterpret_cast<u32*>(currByte), szFileFullPath);
		return result;
	}
	currByte += 4;
	const u32 riffFmtChunkSize = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	if(riffFmtChunkSize != 16)
	{
		KLOG_ERROR("RIFF.fmt.chunkSize==%i invalid/unsupported for '%s'!", 
		           riffFmtChunkSize, szFileFullPath);
		return result;
	}
	const u16 riffFmtAudioFormat = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	if(riffFmtAudioFormat != 1)
	{
		KLOG_ERROR("RIFF.fmt.audioFormat==%i invalid/unsupported for '%s'!",
		           riffFmtAudioFormat, szFileFullPath);
		return result;
	}
	const u16 riffFmtNumChannels = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	if(riffFmtNumChannels > 255)
	{
		KLOG_ERROR("RIFF.fmt.numChannels==%i invalid/unsupported for '%s'!",
		           riffFmtNumChannels, szFileFullPath);
		return result;
	}
	const u32 riffFmtSampleRate = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	if(riffFmtSampleRate != 44100)
	{
		KLOG_ERROR("RIFF.fmt.riffFmtSampleRate==%i invalid/unsupported for "
		           "'%s'!", riffFmtSampleRate, szFileFullPath);
		return result;
	}
	const u32 riffFmtByteRate = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	const u16 riffFmtBlockAlign = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	const u16 riffFmtBitsPerSample = *reinterpret_cast<u16*>(currByte);
	currByte += 2;
	if(riffFmtBitsPerSample != sizeof(SoundSample)*8)
	{
		KLOG_ERROR("RIFF.fmt.riffFmtBitsPerSample==%i invalid/unsupported for "
		           "'%s'!", riffFmtBitsPerSample, szFileFullPath);
		return result;
	}
	if(riffFmtByteRate != 
		riffFmtSampleRate * riffFmtNumChannels * riffFmtBitsPerSample/8)
	{
		KLOG_ERROR("RIFF.fmt.riffFmtByteRate==%i invalid for '%s'!", 
		           riffFmtByteRate, szFileFullPath);
		return result;
	}
	if(riffFmtBlockAlign != riffFmtNumChannels * riffFmtBitsPerSample/8)
	{
		KLOG_ERROR("RIFF.fmt.riffFmtBlockAlign==%i invalid for '%s'!", 
		           riffFmtBlockAlign, szFileFullPath);
		return result;
	}
	u32Chars.chars[0] = 'd';
	u32Chars.chars[1] = 'a';
	u32Chars.chars[2] = 't';
	u32Chars.chars[3] = 'a';
	if(u32Chars.uInt != *reinterpret_cast<u32*>(currByte))
	{
		KLOG_ERROR("RIFF.data.id==0x%x invalid for '%s'!", 
		           *reinterpret_cast<u32*>(currByte), szFileFullPath);
		return result;
	}
	currByte += 4;
	const u32 riffDataSize = *reinterpret_cast<u32*>(currByte);
	currByte += 4;
	// Now we can extract the actual sound data from the WAV file //
	if(currByte + riffDataSize > 
		reinterpret_cast<u8*>(file.data) + file.dataBytes)
	{
		KLOG_ERROR("RIFF.data.size==%i invalid for '%s'!", 
		           riffDataSize, szFileFullPath);
		return result;
	}
	SoundSample*const sampleData = 
		reinterpret_cast<SoundSample*>(kgaAlloc(kgaHandle, riffDataSize));
	if(!sampleData)
	{
		KLOG_ERROR("Failed to allocate %i bytes for sample data!", 
		           riffDataSize);
		return result;
	}
	memcpy(sampleData, currByte, riffDataSize);
	// Assemble & return a valid result! //
	result.channelCount  = static_cast<u8>(riffFmtNumChannels);
	result.sampleHz      = riffFmtSampleRate;
	result.bitsPerSample = riffFmtBitsPerSample;
	result.sampleData    = sampleData;
	return result;
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
	local_persist const RawImage RESULT_FAILURE = {};
	const i32 tempImageDataBytes = kmath::safeTruncateI32(
		z85::decodedFileSizeBytes(z85ImageNumBytes));
	i8*const tempImageDataBuffer = reinterpret_cast<i8*>(
		kgaAlloc(g_genAllocStbImage, tempImageDataBytes));
	if(!tempImageDataBuffer)
	{
		KLOG_ERROR("Failed to allocate temp image data buffer!");
		return RESULT_FAILURE;
	}
	defer(kgaFree(g_genAllocStbImage, tempImageDataBuffer));
	if(!z85::decode(reinterpret_cast<const i8*>(z85PngData), 
	                tempImageDataBuffer))
	{
		KLOG_ERROR("z85::decode failure!");
		return RESULT_FAILURE;
	}
	i32 imgW, imgH, imgNumByteChannels;
	u8*const img = 
		stbi_load_from_memory(reinterpret_cast<u8*>(tempImageDataBuffer), 
		                      tempImageDataBytes, 
		                      &imgW, &imgH, &imgNumByteChannels, 4);
	kassert(img);
	if(!img)
	{
		KLOG_ERROR("stbi_load_from_memory failure!");
		return RESULT_FAILURE;
	}
	return RawImage{
		.sizeX     = kmath::safeTruncateU32(imgW),
		.sizeY     = kmath::safeTruncateU32(imgH), 
		.pixelData = img };
}
internal PLATFORM_FREE_RAW_IMAGE(platformFreeRawImage)
{
	stbi_image_free(rawImage.pixelData);
	rawImage.pixelData = nullptr;
}
internal PLATFORM_LOG(platformLog)
{
	// Get a timestamp (Source: https://stackoverflow.com/a/35260146) //
	time_t now = time(nullptr);
	struct tm newTime;
	localtime_s(&newTime, &now);
	char timeBuffer[80];
	strftime(timeBuffer, sizeof(timeBuffer), 
#if INTERNAL_BUILD
	         "%M.%S", 
#else
	         "%H:%M.%S", 
#endif
	         &newTime);
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
			KLOG_ERROR("Failed to build fullPathToNewLog!  "
			           "g_pathTemp='%s' fileNameLog='%s'", 
			           g_pathTemp, fileNameLog);
			return;
		}
		if(FAILED(StringCchPrintf(fullPathToLog, MAX_PATH, 
		                          TEXT("%s\\%s"), g_pathTemp, fileNameLog)))
		{
			KLOG_ERROR("Failed to build fullPathToLog!  "
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
		KLOG_ERROR("Failed to create file handle! GetLastError=%i",
		           GetLastError());
		return result;
	}
	defer({
		if(!CloseHandle(fileHandle))
		{
			KLOG_ERROR("Failed to close file handle! GetLastError=%i",
			           GetLastError());
			result.data = nullptr;
		}
	});
	u32 fileSize32;
	{
		LARGE_INTEGER fileSize;
		if(!GetFileSizeEx(fileHandle, &fileSize))
		{
			KLOG_ERROR("Failed to get file size! GetLastError=%i",
			           GetLastError());
			return result;
		}
		fileSize32 = kmath::safeTruncateU32(fileSize.QuadPart);
		if(fileSize32 != fileSize.QuadPart)
		{
			KLOG_ERROR("File size exceeds 32 bits!");
			return result;
		}
	}
	result.data = VirtualAlloc(nullptr, fileSize32, 
	                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if(!result.data)
	{
		KLOG_ERROR("Failed to allocate %i bytes to read file! GetLastError=%i", 
		           fileSize32, GetLastError());
		return result;
	}
	defer({
		if(result.dataBytes != fileSize32 &&
			!VirtualFree(result.data, 0, MEM_RELEASE))
		{
			KLOG_ERROR("Failed to VirtualFree! GetLastError=%i", 
			           GetLastError());
		}
		result.data = nullptr;
	});
	DWORD numBytesRead;
	if(!ReadFile(fileHandle, result.data, fileSize32, &numBytesRead, nullptr))
	{
		KLOG_ERROR("Failed to read file! GetLastError=%i", 
		           fileSize32, GetLastError());
		return result;
	}
	if(numBytesRead != fileSize32)
	{
		KLOG_ERROR("Failed to read file! bytes read: %i / %i", 
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
		KLOG_ERROR("Failed to free file memory! GetLastError=%i", 
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
		KLOG_ERROR("Failed to create write file handle '%s'! GetLastError=%i", 
		           fileName, GetLastError());
		return false;
	}
	bool result = true;
	defer({
		if(!CloseHandle(fileHandle))
		{
			KLOG_ERROR("Failed to close file handle '%s'! GetLastError=%i", 
			           fileName, GetLastError());
			result = false;
		}
	});
	DWORD bytesWritten;
	if(!WriteFile(fileHandle, data, dataBytes, &bytesWritten, nullptr))
	{
		KLOG_ERROR("Failed to write to file '%s'! GetLastError=%i", 
		           fileName, GetLastError());
		return false;
	}
	if(bytesWritten != dataBytes)
	{
		KLOG_ERROR("Failed to completely write to file '%s'! "
		           "Bytes written: %i / %i", 
		           fileName, bytesWritten, dataBytes);
		return false;
	}
	return result;
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
		KLOG_WARNING("Failed to get last write time of file '%s'! "
		             "GetLastError=%i", fileName, GetLastError());
		return result;
	}
	result = fileAttributeData.ftLastWriteTime;
	return result;
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
		KLOG_WARNING("Failed to copy file '%s' to '%s'! GetLastError=%i", 
		             fileNameDll, fileNameDllTemp, GetLastError());
	}
	else
	{
		result.hLib = LoadLibraryA(fileNameDllTemp);
		if(!result.hLib)
		{
			KLOG_ERROR("Failed to load library '%s'! GetLastError=%i", 
			           fileNameDllTemp, GetLastError());
		}
	}
	if(result.hLib)
	{
		result.initialize = reinterpret_cast<fnSig_gameInitialize*>(
			GetProcAddress(result.hLib, "gameInitialize"));
		if(!result.initialize)
		{
			KLOG_ERROR("Failed to get proc address 'gameInitialize'! "
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
			KLOG_WARNING("Failed to get proc address 'gameOnReloadCode'! "
			             "GetLastError=%i", GetLastError());
		}
		result.renderAudio = reinterpret_cast<fnSig_gameRenderAudio*>(
			GetProcAddress(result.hLib, "gameRenderAudio"));
		if(!result.renderAudio)
		{
			KLOG_ERROR("Failed to get proc address 'gameRenderAudio'! "
			           "GetLastError=%i", GetLastError());
		}
		result.updateAndDraw = reinterpret_cast<fnSig_gameUpdateAndDraw*>(
			GetProcAddress(result.hLib, "gameUpdateAndDraw"));
		if(!result.updateAndDraw)
		{
			KLOG_ERROR("Failed to get proc address 'gameUpdateAndDraw'! "
			           "GetLastError=%i", GetLastError());
		}
		result.isValid = (result.initialize && result.renderAudio && 
		                  result.updateAndDraw);
	}
	if(!result.isValid)
	{
		KLOG_WARNING("Game code is invalid!");
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
			KLOG_ERROR("Failed to free game code dll! GetLastError=", 
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
		KLOG_ERROR("Failed to get window dimensions! GetLastError=%i", 
		           GetLastError());
		return W32Dimension2d{.width=0, .height=0};
	}
}
internal void w32ProcessKeyEvent(MSG& msg, GameKeyboard& gameKeyboard)
{
	// Virtual-Key Codes: 
	//	https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	const WPARAM vKeyCode = msg.wParam;
#if SLOW_BUILD
	KLOG_INFO("vKeyCode=%i,0x%x", static_cast<int>(vKeyCode), 
	          static_cast<int>(vKeyCode));
#endif
	const bool keyDown     = (msg.lParam & (1<<31)) == 0;
	const bool keyDownPrev = (msg.lParam & (1<<30)) != 0;
#if 0
	const bool altKeyDown  = (msg.lParam & (1<<29)) != 0;
#endif
	ButtonState* buttonState = nullptr;
	switch(vKeyCode)
	{
		case VK_OEM_COMMA:
			buttonState = &gameKeyboard.comma; break;
		case VK_OEM_PERIOD:
			buttonState = &gameKeyboard.period; break;
		case VK_OEM_2: 
			buttonState = &gameKeyboard.slashForward; break;
		case VK_OEM_5: 
			buttonState = &gameKeyboard.slashBack; break;
		case VK_OEM_4: 
			buttonState = &gameKeyboard.curlyBraceLeft; break;
		case VK_OEM_6: 
			buttonState = &gameKeyboard.curlyBraceRight; break;
		case VK_OEM_1: 
			buttonState = &gameKeyboard.semicolon; break;
		case VK_OEM_7: 
			buttonState = &gameKeyboard.quote; break;
		case VK_OEM_3: 
			buttonState = &gameKeyboard.grave; break;
		case VK_OEM_MINUS: 
			buttonState = &gameKeyboard.tenkeyless_minus; break;
		case VK_OEM_PLUS: 
			buttonState = &gameKeyboard.equals; break;
		case VK_BACK: 
			buttonState = &gameKeyboard.backspace; break;
		case VK_ESCAPE: 
			buttonState = &gameKeyboard.escape; break;
		case VK_RETURN: 
			buttonState = &gameKeyboard.enter; break;
		case VK_SPACE: 
			buttonState = &gameKeyboard.space; break;
		case VK_TAB: 
			buttonState = &gameKeyboard.tab; break;
		case VK_LSHIFT: 
			buttonState = &gameKeyboard.shiftLeft; break;
		case VK_RSHIFT: 
			buttonState = &gameKeyboard.shiftRight; break;
		case VK_LCONTROL: 
			buttonState = &gameKeyboard.controlLeft; break;
		case VK_RCONTROL: 
			buttonState = &gameKeyboard.controlRight; break;
		case VK_MENU: 
			buttonState = &gameKeyboard.alt; break;
		case VK_UP: 
			buttonState = &gameKeyboard.arrowUp; break;
		case VK_DOWN: 
			buttonState = &gameKeyboard.arrowDown; break;
		case VK_LEFT: 
			buttonState = &gameKeyboard.arrowLeft; break;
		case VK_RIGHT: 
			buttonState = &gameKeyboard.arrowRight; break;
		case VK_INSERT: 
			buttonState = &gameKeyboard.insert; break;
		case VK_DELETE: 
			buttonState = &gameKeyboard.deleteKey; break;
		case VK_HOME: 
			buttonState = &gameKeyboard.home; break;
		case VK_END: 
			buttonState = &gameKeyboard.end; break;
		case VK_PRIOR: 
			buttonState = &gameKeyboard.pageUp; break;
		case VK_NEXT: 
			buttonState = &gameKeyboard.pageDown; break;
		case VK_DECIMAL: 
			buttonState = &gameKeyboard.numpad_period; break;
		case VK_DIVIDE: 
			buttonState = &gameKeyboard.numpad_divide; break;
		case VK_MULTIPLY: 
			buttonState = &gameKeyboard.numpad_multiply; break;
		case VK_SEPARATOR: 
			buttonState = &gameKeyboard.numpad_minus; break;
		case VK_ADD: 
			buttonState = &gameKeyboard.numpad_add; break;
		default:
		{
			if(vKeyCode >= 0x41 && vKeyCode <= 0x5A)
			{
				buttonState = &gameKeyboard.a + (vKeyCode - 0x41);
			}
			else if(vKeyCode >= 0x30 && vKeyCode <= 0x39)
			{
				buttonState = &gameKeyboard.tenkeyless_0 + (vKeyCode - 0x30);
			}
			else if(vKeyCode >= 0x70 && vKeyCode <= 0x7B)
			{
				buttonState = &gameKeyboard.f1 + (vKeyCode - 0x70);
			}
			else if(vKeyCode >= 0x60 && vKeyCode <= 0x69)
			{
				buttonState = &gameKeyboard.numpad_0 + (vKeyCode - 0x60);
			}
		} break;
	}
	if(buttonState)
	{
		*buttonState = keyDown
			? (keyDownPrev
				? ButtonState::HELD
				: ButtonState::PRESSED)
			: ButtonState::NOT_PRESSED;
	}
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
		KLOG_ERROR("Failed to get monitor info!");
		return DEFAULT_RESULT;
	}
	DEVMODEA monitorDevMode;
	monitorDevMode.dmSize = sizeof(DEVMODEA);
	monitorDevMode.dmDriverExtra = 0;
	if(!EnumDisplaySettingsA(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, 
	                         &monitorDevMode))
	{
		KLOG_ERROR("Failed to enum display settings!");
		return DEFAULT_RESULT;
	}
	if(monitorDevMode.dmDisplayFrequency < 2)
	{
		KLOG_WARNING("Unknown hardware-defined refresh rate! "
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
		case WM_SETCURSOR:
		{
			if(g_displayCursor)
			{
				SetCursor(g_cursor);
			}
			else
			{
				SetCursor(NULL);
			}
		} break;
		case WM_SIZE:
		{
			KLOG_INFO("WM_SIZE");
		} break;
		case WM_DESTROY:
		{
			///TODO: handle this error: recreate window?
			g_running = false;
			KLOG_INFO("WM_DESTROY");
		} break;
		case WM_CLOSE:
		{
			///TODO: ask user first before destroying
			g_running = false;
			KLOG_INFO("WM_CLOSE");
		} break;
		case WM_ACTIVATEAPP:
		{
			KLOG_INFO("WM_ACTIVATEAPP");
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
		default:
		{
			result = DefWindowProcA(hwnd, uMsg, wParam, lParam);
		}break;
	}
	return result;
}
internal LARGE_INTEGER w32QueryPerformanceCounter()
{
	LARGE_INTEGER result;
	if(!QueryPerformanceCounter(&result))
	{
		KLOG_ERROR("Failed to query performance counter! GetLastError=%i", 
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
	// I don't use the KLOG_ERROR macro here because `strrchr` from the 
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
	//KLOG_ERROR("Exception! 0x%x", 
	//           pExceptionInfo->ExceptionRecord->ExceptionCode);
	w32WriteLogToFile();
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
extern int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, 
                           PWSTR /*pCmdLine*/, int /*nCmdShow*/)
{
	local_persist const int RETURN_CODE_SUCCESS = 0;
	///TODO: OR the failure code with a more specific # to indicate why it 
	///      failed probably?
	local_persist const int RETURN_CODE_FAILURE = 0xBADC0DE0;
	defer(w32WriteLogToFile());
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
			KLOG_INFO("Previous reserved stack=%ld, new reserved stack=%ld", 
			          stackSizeInBytes, DESIRED_RESERVED_STACK_BYTES);
		}
		else
		{
			KLOG_WARNING("Failed to set & retrieve reserved stack size!  "
			             "The system probably won't be able to log stack "
			             "overflow exceptions.");
		}
	}
#if 0
	if(SetUnhandledExceptionFilter(w32TopLevelExceptionHandler))
	{
		KLOG_INFO("Replaced top level exception filter!");
	}
	else
	{
		KLOG_INFO("Set new top level exception filter!");
	}
#endif
	if(!AddVectoredExceptionHandler(1, w32VectoredExceptionHandler))
	{
		KLOG_WARNING("Failed to add vectored exception handler!");
	}
	KLOG_INFO("START!");
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
				KLOG_ERROR("Failed to get APPDATA path! result=%ld", result);
				return RETURN_CODE_FAILURE;
			}
		}
		if(FAILED(StringCchPrintf(g_pathLocalAppData, MAX_PATH, TEXT("%s\\%s"), 
		                          szPath, APPLICATION_NAME)))
		{
			KLOG_ERROR("Failed to build application APPDATA path!");
			return RETURN_CODE_FAILURE;
		}
		KLOG_INFO("g_pathLocalAppData='%s'", g_pathLocalAppData);
		if(!CreateDirectory(g_pathLocalAppData, NULL))
		{
			const int errorCode = GetLastError();
			switch(errorCode)
			{
				case ERROR_ALREADY_EXISTS:
				{
					KLOG_INFO("Application APPDATA path '%s' already exists.", 
					          g_pathLocalAppData);
				} break;
				default:
				{
					KLOG_ERROR("Failed to create APPDATA path '%s'! "
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
			KLOG_ERROR("Failed to get OS temp path! GetLastError=%i", 
			           GetLastError());
			return RETURN_CODE_FAILURE;
		}
		if(FAILED(StringCchPrintf(g_pathTemp, MAX_PATH, TEXT("%s%s"), szPath, 
		                          APPLICATION_NAME)))
		{
			KLOG_ERROR("Failed to build application temp path!");
			return RETURN_CODE_FAILURE;
		}
		KLOG_INFO("g_pathTemp='%s'", g_pathTemp);
		if(!CreateDirectory(g_pathTemp, NULL))
		{
			const int errorCode = GetLastError();
			switch(errorCode)
			{
				case ERROR_ALREADY_EXISTS:
				{
					KLOG_INFO("Application temp path '%s' already exists.", 
					          g_pathTemp);
				} break;
				default:
				{
					KLOG_ERROR("Failed to create app temp path '%s'! "
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
			KLOG_ERROR("Failed to get exe file path+name! GetLastError=%i", 
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
	///TODO: handle file paths longer than MAX_PATH in the future...
	TCHAR fullPathToGameDll    [MAX_PATH] = {};
	TCHAR fullPathToGameDllTemp[MAX_PATH] = {};
	// determine the FULL path to the game DLL file //
	{
		if(FAILED(StringCchPrintf(fullPathToGameDll, MAX_PATH, 
		                          TEXT("%s\\%s.dll"), g_pathToExe, 
		                          FILE_NAME_GAME_DLL)))
		{
			KLOG_ERROR("Failed to build fullPathToGameDll!  "
			           "g_pathToExe='%s' FILE_NAME_GAME_DLL='%s'", 
			           g_pathToExe, FILE_NAME_GAME_DLL);
			return RETURN_CODE_FAILURE;
		}
		if(FAILED(StringCchPrintf(fullPathToGameDllTemp, MAX_PATH, 
		                          TEXT("%s\\%s_temp.dll"), g_pathTemp, 
		                          FILE_NAME_GAME_DLL)))
		{
			KLOG_ERROR("Failed to build fullPathToGameDllTemp!  "
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
			KLOG_ERROR("Failed to VirtualAlloc %i bytes for STB_IMAGE! "
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
			KLOG_ERROR("Failed to VirtualAlloc %i bytes for ImGui! "
			           "GetLastError=%i", 
			           IMGUI_MEMORY_BYTES, GetLastError());
			return RETURN_CODE_FAILURE;
		}
		g_genAllocImgui = kgaInit(imguiMemory, IMGUI_MEMORY_BYTES);
	}
	GameCode game = w32LoadGameCode(fullPathToGameDll, 
	                                fullPathToGameDllTemp);
	kassert(game.isValid);
	if(!QueryPerformanceFrequency(&g_perfCounterHz))
	{
		KLOG_ERROR("Failed to query for performance frequency! GetLastError=%i", 
		           GetLastError());
		return RETURN_CODE_FAILURE;
	}
	w32LoadXInput();
#if 0
	w32ResizeDibSection(g_backBuffer, 1280, 720);
#endif
#if INTERNAL_BUILD
	g_displayCursor = true;
#endif
	g_cursor = LoadCursorA(NULL, IDC_ARROW);
	const WNDCLASS wndClass = {
		.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
		.lpfnWndProc   = w32MainWindowCallback,
		.hInstance     = hInstance,
		.hCursor       = g_cursor,
		.lpszClassName = "KmlWindowClass" };
	const ATOM atomWindowClass = RegisterClassA(&wndClass);
	if(atomWindowClass == 0)
	{
		KLOG_ERROR("Failed to register WNDCLASS! GetLastError=%i", 
		           GetLastError());
		return RETURN_CODE_FAILURE;
	}
	const HWND mainWindow = CreateWindowExA(
		0,
		wndClass.lpszClassName,
		APPLICATION_NAME,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL );
	if(!mainWindow)
	{
		KLOG_ERROR("Failed to create window! GetLastError=%i", GetLastError());
		return RETURN_CODE_FAILURE;
	}
	w32KrbOglInitialize(mainWindow);
	w32KrbOglSetVSyncPreference(true);
	///TODO: update the monitorRefreshHz and dependent variable realtime when 
	///      the window gets moved around to another monitor.
	u32 monitorRefreshHz = w32QueryNearestMonitorRefreshRate(mainWindow);
	u32 gameUpdateHz = monitorRefreshHz / 2;
	f32 targetSecondsElapsedPerFrame = 1.f / gameUpdateHz;
	GameKeyboard gameKeyboard = {};
#if INTERNAL_BUILD
	// ensure that the size of the keyboard's vKeys array matches the size of
	//	the anonymous struct which defines the names of all the game keys //
	kassert(static_cast<size_t>(&gameKeyboard.DUMMY_LAST_BUTTON_STATE - 
	                            &gameKeyboard.vKeys[0]) ==
	        (sizeof(gameKeyboard.vKeys) / sizeof(gameKeyboard.vKeys[0])));
#endif
	GamePad gamePadArrayA[XUSER_MAX_COUNT] = {};
	GamePad gamePadArrayB[XUSER_MAX_COUNT] = {};
	GamePad* gamePadArrayCurrentFrame  = gamePadArrayA;
	GamePad* gamePadArrayPreviousFrame = gamePadArrayB;
	u8 numGamePads = 0;
	local_persist const u8 SOUND_CHANNELS = 2;
	local_persist const u32 SOUND_SAMPLE_HZ = 44100;
	local_persist const u32 SOUND_BYTES_PER_SAMPLE = sizeof(i16)*SOUND_CHANNELS;
	local_persist const u32 SOUND_BUFFER_BYTES = 
		SOUND_SAMPLE_HZ * SOUND_BYTES_PER_SAMPLE;
	local_persist const u32 SOUND_LATENCY_SAMPLES = SOUND_SAMPLE_HZ / 15;
	u32 runningSoundSample = 0;
	VOID*const gameSoundMemory = VirtualAlloc(NULL, SOUND_BUFFER_BYTES, 
	                                          MEM_RESERVE | MEM_COMMIT, 
	                                          PAGE_READWRITE);
	if(!gameSoundMemory)
	{
		KLOG_ERROR("Failed to VirtualAlloc %i bytes for sound memory! "
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
			KLOG_ERROR("ImGui_ImplOpenGL2_Init failure!");
			return RETURN_CODE_FAILURE;
		}
		if(!ImGui_ImplWin32_Init(mainWindow))
		{
			KLOG_ERROR("ImGui_ImplWin32_Init failure!");
			return RETURN_CODE_FAILURE;
		}
	}
	GameMemory gameMemory = {};
#if INTERNAL_BUILD
	const LPVOID baseAddress = reinterpret_cast<LPVOID>(kmath::terabytes(1));
#else
	const LPVOID baseAddress = 0;
#endif
	gameMemory.permanentMemoryBytes = kmath::megabytes(64);
	gameMemory.transientMemoryBytes = kmath::gigabytes(4);
	const u64 totalGameMemoryBytes = 
		gameMemory.permanentMemoryBytes + gameMemory.transientMemoryBytes;
	gameMemory.permanentMemory = VirtualAlloc(baseAddress, 
	                                          totalGameMemoryBytes, 
	                                          MEM_RESERVE | MEM_COMMIT, 
	                                          PAGE_READWRITE);
	if(!gameMemory.permanentMemory)
	{
		KLOG_ERROR("Failed to VirtualAlloc %i bytes for game memory! "
		           "GetLastError=%i", totalGameMemoryBytes, GetLastError());
		return RETURN_CODE_FAILURE;
	}
	gameMemory.transientMemory = 
		reinterpret_cast<u8*>(gameMemory.permanentMemory) + 
		gameMemory.permanentMemoryBytes;
	gameMemory.platformLog              = platformLog;
	gameMemory.platformDecodeZ85Png     = platformDecodeZ85Png;
	gameMemory.platformFreeRawImage     = platformFreeRawImage;
	gameMemory.platformLoadWav          = platformLoadWav;
#if INTERNAL_BUILD
	gameMemory.platformReadEntireFile   = platformReadEntireFile;
	gameMemory.platformFreeFileMemory   = platformFreeFileMemory;
	gameMemory.platformWriteEntireFile  = platformWriteEntireFile;
#endif// INTERNAL_BUILD
	gameMemory.krbBeginFrame            = krbBeginFrame;
	gameMemory.krbSetProjectionOrtho    = krbSetProjectionOrtho;
	gameMemory.krbDrawLine              = krbDrawLine;
	gameMemory.krbDrawTri               = krbDrawTri;
	gameMemory.krbDrawTriTextured       = krbDrawTriTextured;
	gameMemory.krbViewTranslate         = krbViewTranslate;
	gameMemory.krbSetModelXform         = krbSetModelXform;
	gameMemory.krbLoadImage             = krbLoadImage;
	gameMemory.krbUseTexture            = krbUseTexture;
	gameMemory.imguiContext             = ImGui::GetCurrentContext();
	gameMemory.platformImguiAlloc       = platformImguiAlloc;
	gameMemory.platformImguiFree        = platformImguiFree;
	gameMemory.imguiAllocUserData       = g_genAllocImgui;
	game.onReloadCode(gameMemory);
	game.initialize(gameMemory);
	w32InitDSound(mainWindow, SOUND_SAMPLE_HZ, SOUND_BUFFER_BYTES, 
	              SOUND_CHANNELS, SOUND_LATENCY_SAMPLES, runningSoundSample);
	const HDC hdc = GetDC(mainWindow);
	if(!hdc)
	{
		KLOG_ERROR("Failed to get main window device context!");
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
			KLOG_WARNING("System doesn't support custom scheduler granularity. "
			             "Main thread will not be put to sleep!");
		}
	}
	// set scheduler granularity using timeBeginPeriod //
	const bool sleepIsGranular = systemSupportsDesiredTimerGranularity &&
		timeBeginPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) == TIMERR_NOERROR;
	if(!sleepIsGranular)
	{
		KLOG_WARNING("System supports custom scheduler granularity, but "
		             "setting this value has failed! Main thread will not be "
		             "put to sleep!");
	}
	defer({
		if (sleepIsGranular && 
			timeEndPeriod(DESIRED_OS_TIMER_GRANULARITY_MS) != TIMERR_NOERROR)
		{
			KLOG_ERROR("Failed to timeEndPeriod!");
		}
	});
	// Set the process to a higher priority to minimize the chance of another 
	//	process keeping us asleep hopefully. //
	if(sleepIsGranular)
	{
		const HANDLE hProcess = GetCurrentProcess();
		if(!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS))
		{
			KLOG_ERROR("Failed to set the process priority class to "
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
				if((CompareFileTime(&gameCodeLastWriteTime, 
				                    &game.dllLastWriteTime) &&
					!(gameCodeLastWriteTime.dwHighDateTime == 0 &&
					gameCodeLastWriteTime.dwLowDateTime    == 0)) ||
					!game.isValid)
				{
					w32UnloadGameCode(game);
					game = w32LoadGameCode(fullPathToGameDll, 
					                       fullPathToGameDllTemp);
					if(game.isValid)
					{
						KLOG_INFO("Game code hot-reload complete!");
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
#if 0
					case WM_CHAR:
					{
#if SLOW_BUILD
						KLOG_INFO("character code=%i", 
						          static_cast<int>(windowMessage.wParam));
#endif
					} break;
#endif
					case WM_KEYDOWN:
					case WM_KEYUP:
					case WM_SYSKEYDOWN:
					case WM_SYSKEYUP:
						w32ProcessKeyEvent(windowMessage, gameKeyboard);
					default:
					{
						TranslateMessage(&windowMessage);
						DispatchMessageA(&windowMessage);
					} break;
				}
			}
			gameKeyboard.modifiers.shift =
				(gameKeyboard.shiftLeft  > ButtonState::NOT_PRESSED ||
				 gameKeyboard.shiftRight > ButtonState::NOT_PRESSED);
			gameKeyboard.modifiers.control =
				(gameKeyboard.controlLeft  > ButtonState::NOT_PRESSED ||
				 gameKeyboard.controlRight > ButtonState::NOT_PRESSED);
			gameKeyboard.modifiers.alt =
				gameKeyboard.alt > ButtonState::NOT_PRESSED;
			if(gamePadArrayCurrentFrame == gamePadArrayA)
			{
				gamePadArrayPreviousFrame = gamePadArrayA;
				gamePadArrayCurrentFrame  = gamePadArrayB;
			}
			else
			{
				gamePadArrayPreviousFrame = gamePadArrayB;
				gamePadArrayCurrentFrame  = gamePadArrayA;
			}
			w32XInputGetGamePadStates(&numGamePads,
			                          gamePadArrayCurrentFrame,
			                          gamePadArrayPreviousFrame);
			ImGui_ImplOpenGL2_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
#if 0
			ImGui::ShowDemoWindow();
#endif
			const W32Dimension2d windowDims = 
				w32GetWindowDimensions(mainWindow);
			if(!game.updateAndDraw(gameMemory, 
			                       {windowDims.width, windowDims.height}, 
			                       gameKeyboard,
			                       gamePadArrayCurrentFrame, numGamePads))
			{
				g_running = false;
			}
			w32WriteDSoundAudio(SOUND_BUFFER_BYTES, SOUND_SAMPLE_HZ, 
			                    SOUND_CHANNELS, SOUND_LATENCY_SAMPLES, 
			                    runningSoundSample, gameSoundMemory, 
			                    gameMemory, game);
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
			// update window graphics //
			ImGui::Render();
			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
			if(!SwapBuffers(hdc))
			{
				KLOG_ERROR("Failed to SwapBuffers! GetLastError=%i", 
				           GetLastError());
			}
			// enforce targetSecondsElapsedPerFrame //
			///TODO: we still have to Sleep/wait when VSync is on if SwapBuffers
			///      completes too early!!! (like when the double-buffer is not
			///      yet filled up at beginning of execution)
			///TODO: maybe just don't sleep/wait the first frame at all because
			///      the windows message queue needs to be unclogged and game
			///      state memory needs to be initialized.
			if(!w32KrbOglGetVSync())
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
#if SLOW_BUILD
				// send performance measurement to debugger as a string //
				KLOG_INFO("%.2f ms/f | %.2f Mc/f", 
				          elapsedSeconds*1000, clockCycleDiff/1000000.f);
#endif
				perfCountPrev   = perfCount;
				clockCyclesPrev = clockCycles;
			}
		}
	}
	kassert(kgaUsedBytes(g_genAllocStbImage) == 0);
	KLOG_INFO("END! :)");
	return RETURN_CODE_SUCCESS;
}
#include "win32-dsound.cpp"
#include "win32-xinput.cpp"
#include "win32-krb-opengl.cpp"
#include "krb-opengl.cpp"
#include "z85.cpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ASSERT(x) kassert(x)
#define STBI_MALLOC(sz)       kgaAlloc(g_genAllocStbImage,sz)
#define STBI_REALLOC(p,newsz) kgaRealloc(g_genAllocStbImage,p,newsz)
#define STBI_FREE(p)          kgaFree(g_genAllocStbImage,p)
#include "stb/stb_image.h"
#include "generalAllocator.cpp"
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