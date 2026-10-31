#pragma once

#include <cstdint>
#include <cstdio>
#include "../../include/DeyeRegisters.h"
#include "RegisterCache.h"
#include "../util/StrAppend.h"

// Shared decoding of register words into engineering values, used by both
// the MQTT publisher and the web status endpoint.
namespace DeyeBridge
{
    inline float EntityValue(const DeyeRegisters::SensorEntity &entity, const RegisterCache &cache)
    {
        if (entity.isDword)
        {
            const uint32_t raw = static_cast<uint32_t>(cache.Get(entity.address)) |
                                 (static_cast<uint32_t>(cache.Get(entity.address + 1)) << 16);
            return static_cast<float>(static_cast<int32_t>(raw)) * entity.scale + entity.offset;
        }
        if (entity.isSigned)
        {
            return static_cast<float>(static_cast<int16_t>(cache.Get(entity.address))) * entity.scale + entity.offset;
        }
        return static_cast<float>(cache.Get(entity.address)) * entity.scale + entity.offset;
    }

    inline int EntityDecimals(const DeyeRegisters::SensorEntity &entity)
    {
        const float absScale = entity.scale < 0 ? -entity.scale : entity.scale;
        return absScale >= 1.0f ? 0 : (absScale >= 0.1f ? 1 : 2);
    }

    // Compact JSON snapshot of every valid register, shared by the MQTT state
    // topic and the web /values endpoint. Truncates safely at the buffer end.
    inline void BuildStateJson(const RegisterCache &cache, char *out, size_t size)
    {
        size_t n = 0;
        bool first = true;
        Util::StrAppend(out, size, n, "{");

        char value[24];
        for (size_t i = 0; i < DeyeRegisters::kSensorCount; ++i)
        {
            const DeyeRegisters::SensorEntity &e = DeyeRegisters::kSensors[i];
            if (!cache.IsValid(e.address))
            {
                continue;
            }
            snprintf(value, sizeof(value), "%.*f", EntityDecimals(e), EntityValue(e, cache));
            Util::StrAppend(out, size, n, "%s\"%s\":%s", first ? "" : ",", e.key, value);
            first = false;
        }
        for (size_t i = 0; i < DeyeRegisters::kBinaryCount; ++i)
        {
            const DeyeRegisters::BinaryEntity &e = DeyeRegisters::kBinaries[i];
            if (!cache.IsValid(e.address))
            {
                continue;
            }
            Util::StrAppend(out, size, n, "%s\"%s\":%s",
                            first ? "" : ",", e.key, cache.Get(e.address) ? "true" : "false");
            first = false;
        }
        for (size_t i = 0; i < DeyeRegisters::kTimeSlotCount; ++i)
        {
            const DeyeRegisters::TextEntity &e = DeyeRegisters::kTimeSlots[i];
            if (!cache.IsValid(e.address))
            {
                continue;
            }
            const uint16_t raw = cache.Get(e.address);
            Util::StrAppend(out, size, n, "%s\"%s\":\"%02u:%02u\"",
                            first ? "" : ",", e.key,
                            static_cast<unsigned>(raw / 100), static_cast<unsigned>(raw % 100));
            first = false;
        }
        if (cache.IsValid(672))
        {
            const int32_t total = static_cast<int32_t>(cache.Get(672)) +
                                  cache.Get(673) + cache.Get(674) + cache.Get(675);
            Util::StrAppend(out, size, n, "%s\"solar_power\":%ld",
                            first ? "" : ",", static_cast<long>(total));
            first = false;
        }
        if (cache.IsValid(500))
        {
            Util::StrAppend(out, size, n, "%s\"overall_state\":\"%s\"",
                            first ? "" : ",", DeyeRegisters::OverallStateName(cache.Get(500)));
        }
        Util::StrAppend(out, size, n, "}");
    }
}
