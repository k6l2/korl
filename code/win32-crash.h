#pragma once
#include "win32-main.h"
internal int w32GenerateDump(PEXCEPTION_POINTERS pExceptionPointers);
internal LONG WINAPI 
	w32VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo);