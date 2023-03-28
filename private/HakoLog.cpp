#include "HakoLog.h"

#include <cstdio>
#include <cstdarg>

void hako::Log(char const* a_Format, ...)
{
#if !defined(HAKO_NO_LOG)
    printf("[Hako] ");
    va_list args;
    va_start(args, a_Format);
    vprintf(a_Format, args);
    va_end(args);
#endif
}

void hako::Log(wchar_t const* a_Format, ...)
{
#if !defined(HAKO_NO_LOG)
    printf("[Hako] ");
    va_list args;
    va_start(args, a_Format);
    vwprintf(a_Format, args);
    va_end(args);
#endif
}
