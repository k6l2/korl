#pragma once
#include "win32-main.h"
global_variable LARGE_INTEGER g_perfCounterHz;
internal LARGE_INTEGER w32QueryPerformanceCounter();
internal FILETIME w32GetLastWriteTime(const char* fileName);
internal f32 
	w32ElapsedSeconds(
		const LARGE_INTEGER& previousPerformanceCount, 
		const LARGE_INTEGER& performanceCount);