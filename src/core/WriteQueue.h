#pragma once

#include <stdint.h>
#include <stddef.h>

// FIFO of pending register writes. Every writer (Modbus TCP client such as
// evcc, MQTT command topics such as Home Assistant) pushes here; the RTU
// poller drains the queue between block reads so all bus traffic stays
// serialized in one task.
namespace DeyeBridge
{
    struct WriteEntry
    {
        uint16_t address;
        uint16_t value;
    };

    class WriteQueue
    {
    public:
        bool Push(uint16_t address, uint16_t value);
        bool Pop(WriteEntry &out);
        bool Empty() const { return mSize == 0; }
        size_t Size() const { return mSize; }

        static constexpr size_t kCapacity = 64;

    private:
        WriteEntry mEntries[kCapacity];
        size_t mHead = 0;   // next pop
        size_t mTail = 0;   // next push
        size_t mSize = 0;
    };
}
