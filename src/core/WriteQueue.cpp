#include "WriteQueue.h"

namespace DeyeBridge
{
    bool WriteQueue::Push(uint16_t address, uint16_t value, uint16_t clearMask)
    {
        if (mSize >= kCapacity)
        {
            return false;
        }
        mEntries[mTail].address = address;
        mEntries[mTail].value = value;
        mEntries[mTail].clearMask = clearMask == 0 ? 0xFFFF : clearMask;
        mTail = (mTail + 1) % kCapacity;
        ++mSize;
        return true;
    }

    bool WriteQueue::Pop(WriteEntry &out)
    {
        if (mSize == 0)
        {
            return false;
        }
        out = mEntries[mHead];
        mHead = (mHead + 1) % kCapacity;
        --mSize;
        return true;
    }
}
