#pragma once

#include <cstddef>
#include <cstring>

// Escapes &, <, >, " and ' for safe use inside HTML text and attributes.
// Output is always NUL-terminated and never overruns the buffer.
namespace Util
{
    inline void HtmlEscape(const char *src, char *out, size_t size)
    {
        if (size == 0)
        {
            return;
        }
        size_t o = 0;
        for (const char *p = src; *p != '\0' && o + 1 < size; ++p)
        {
            size_t need = 1;
            const char *rep = nullptr;
            switch (*p)
            {
                case '&': rep = "&amp;"; need = 5; break;
                case '<': rep = "&lt;"; need = 4; break;
                case '>': rep = "&gt;"; need = 4; break;
                case '"': rep = "&quot;"; need = 6; break;
                case '\'': rep = "&#39;"; need = 5; break;
                default: break;
            }
            if (rep)
            {
                if (o + need >= size)
                {
                    break;
                }
                memcpy(out + o, rep, need);
                o += need;
            }
            else
            {
                out[o++] = *p;
            }
        }
        out[o] = '\0';
    }
}
