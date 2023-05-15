#include "korl-string.h"
#include "utility/korl-checkCast.h"
#include "korl-interface-platform-memory.h"
#include <stdlib.h>// for strtof
#include <ctype.h>// for toupper
korl_internal KORL_FUNCTION_korl_string_utf8_to_f32(korl_string_utf8_to_f32)
{
    char* endPointer = NULL;
    errno = 0;
    const f32 result = strtof(KORL_C_CAST(const char*, utf8.data), &endPointer);
    if(out_utf8F32End)
    {
        *out_utf8F32End = KORL_C_CAST(u8*, endPointer);
        korl_assert(*out_utf8F32End < utf8.data + utf8.size);
    }
    if(out_resultIsValid)
        *out_resultIsValid = (errno == 0);
    return result;
}
korl_internal KORL_FUNCTION_korl_string_formatVaListUtf8(korl_string_formatVaListUtf8)
{
    const int bufferSize = _vscprintf(format, vaList) + 1/*for the null terminator*/;
    korl_assert(bufferSize > 0);
    char*const result = (char*)korl_allocate(allocatorHandle, bufferSize * sizeof(*result));
    korl_assert(result);
    const int charactersWritten = vsprintf_s(result, bufferSize, format, vaList);
    korl_assert(charactersWritten == bufferSize - 1);
    return result;
}
korl_internal KORL_FUNCTION_korl_string_formatVaListUtf16(korl_string_formatVaListUtf16)
{
    const int bufferSize = _vscwprintf(format, vaList) + 1/*for the null terminator*/;
    korl_assert(bufferSize > 0);
    wchar_t*const result = (wchar_t*)korl_allocate(allocatorHandle, bufferSize * sizeof(*result));
    korl_assert(result);
    const int charactersWritten = vswprintf_s(result, bufferSize, format, vaList);
    korl_assert(charactersWritten == bufferSize - 1);
    return result;
}
korl_internal KORL_FUNCTION_korl_string_formatBufferVaListUtf16(korl_string_formatBufferVaListUtf16)
{
    const int bufferSize = _vscwprintf(format, vaList);// excludes null terminator
    korl_assert(bufferSize >= 0);
    if(korl_checkCast_i$_to_u$(bufferSize + 1/*for null terminator*/) > bufferBytes / sizeof(*buffer))
        return -(bufferSize + 1/*for null terminator*/);
    const int charactersWritten = vswprintf_s(buffer, bufferBytes/sizeof(*buffer), format, vaList);// excludes null terminator
    korl_assert(charactersWritten == bufferSize);
    return charactersWritten + 1/*for null terminator*/;
}
korl_internal KORL_FUNCTION_korl_string_unicode_toUpper(korl_string_unicode_toUpper)
{
    return towupper(korl_checkCast_u$_to_u16(codePoint));
}
