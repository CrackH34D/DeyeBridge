#include "WriteQueue.h"

namespace DeyeBridge
{
    bool WriteQueue::Push(uint16_t address, uint16_t value)
    {
        if (mSize >= kCapacity)
        {
            return false;
        }
        mEntries[mTail].address = address;
        mEntries[mTail].value = value;
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
