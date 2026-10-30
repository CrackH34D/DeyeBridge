#pragma once

#include <stdint.h>

// Register image of the whole Deye holding-register window. Values are stored
// as raw 16-bit words exactly as they arrive on the bus, so Modbus TCP clients
// (evcc) get a 1:1 pass-through including 32-bit word order.
namespace DeyeBridge
{
    class RegisterCache
    {
    public:
        explicit RegisterCache(uint16_t spaceEnd);

        // Stores a block read from the bus and marks the words valid
        void Store(uint16_t start, const uint16_t *values, uint16_t count);

        // Raw word; returns 0 for addresses outside the window
        uint16_t Get(uint16_t address) const;

        // Optimistic mirror of a queued write so reads reflect the command
        // immediately (until the next poll overwrites it with the real value)
        void Poke(uint16_t address, uint16_t value);

        bool IsValid(uint16_t address) const;
        bool AllValid() const { return mValidCount > 0 && mValidCount == mSpaceEnd; }

        uint16_t SpaceEnd() const { return mSpaceEnd; }

    private:
        uint16_t mSpaceEnd;
        uint16_t *mRegs;
        bool *mValid;
        uint32_t mValidCount = 0;
    };
}
