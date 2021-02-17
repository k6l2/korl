#include "win32-time.h"
internal FILETIME w32GetLastWriteTime(const char* fileName)
{
	FILETIME result = {};
	WIN32_FILE_ATTRIBUTE_DATA fileAttributeData;
	if(!GetFileAttributesExA(
			fileName, GetFileExInfoStandard, &fileAttributeData))
	{
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
		/* if the file can't be found, just silently return a zero FILETIME */
		{
			return result;
		}
		KLOG(WARNING, "Failed to get last write time of file '%s'! "
		     "GetLastError=%i", fileName, GetLastError());
		return result;
	}
	result = fileAttributeData.ftLastWriteTime;
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
internal f32 
	w32ElapsedSeconds(
		const LARGE_INTEGER& previousPerformanceCount, 
		const LARGE_INTEGER& performanceCount)
{
	korlAssert(performanceCount.QuadPart > previousPerformanceCount.QuadPart);
	const LONGLONG perfCountDiff = 
		performanceCount.QuadPart - previousPerformanceCount.QuadPart;
	const f32 elapsedSeconds = 
		static_cast<f32>(perfCountDiff) / g_perfCounterHz.QuadPart;
	return elapsedSeconds;
}