#pragma once

#include <Arduino.h>

// Tiny printf-style serial logger
namespace Log
{
    inline void Emit(const char *level, const char *format, va_list args)
    {
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Serial.printf("[%lu][%s] %s\n", static_cast<unsigned long>(millis()), level, buffer);
    }

    inline void info(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        Emit("INFO", format, args);
        va_end(args);
    }

    inline void warn(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        Emit("WARN", format, args);
        va_end(args);
    }

    inline void error(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        Emit("ERR ", format, args);
        va_end(args);
    }
}
