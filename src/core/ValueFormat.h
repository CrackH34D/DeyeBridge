#pragma once

#include "../../include/DeyeRegisters.h"
#include "RegisterCache.h"

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
}
