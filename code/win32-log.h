#pragma once
#include "kutil.h"
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
internal void w32WriteLogToFile();