#pragma once

#include <cstdint>
#include <cstddef>

// Modbus RTU CRC16 (poly 0xA001), header-only so host tests can use it too
namespace Rtu
{
    inline uint16_t Crc16(const uint8_t *data, size_t length)
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; ++i)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                if (crc & 1)
                {
                    crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
                }
                else
                {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }
}
