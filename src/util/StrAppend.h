#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdio>

// snprintf-style append into a fixed buffer that can never overrun: pos is
// clamped to the last valid index once the buffer is full, so the output is
// truncated at most and stays NUL-terminated.
namespace Util
{
    inline void StrAppend(char *buf, size_t size, size_t &pos, const char *fmt, ...)
    {
        if (size == 0 || pos >= size - 1)
        {
            return;
        }
        va_list args;
        va_start(args, fmt);
        const int written = vsnprintf(buf + pos, size - pos, fmt, args);
        va_end(args);
        if (written > 0)
        {
            pos += static_cast<size_t>(written);
            if (pos > size - 1)
            {
                pos = size - 1;
            }
        }
    }
}
