#include "RegisterCache.h"

namespace DeyeBridge
{
    RegisterCache::RegisterCache(uint16_t spaceEnd)
        : mSpaceEnd(spaceEnd)
    {
        mRegs = new uint16_t[mSpaceEnd]();
        mValid = new bool[mSpaceEnd]();
    }

    void RegisterCache::Store(uint16_t start, const uint16_t *values, uint16_t count)
    {
        for (uint16_t i = 0; i < count; ++i)
        {
            const uint16_t address = start + i;
            if (address >= mSpaceEnd)
            {
                break;
            }
            if (!mValid[address])
            {
                mValid[address] = true;
                ++mValidCount;
            }
            mRegs[address] = values[i];
        }
    }

    uint16_t RegisterCache::Get(uint16_t address) const
    {
        return address < mSpaceEnd ? mRegs[address] : 0;
    }

    void RegisterCache::Poke(uint16_t address, uint16_t value)
    {
        if (address < mSpaceEnd)
        {
            mRegs[address] = value;
        }
    }

    bool RegisterCache::IsValid(uint16_t address) const
    {
        return address < mSpaceEnd && mValid[address];
    }
}
