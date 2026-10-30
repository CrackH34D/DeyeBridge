#pragma once

#include <Arduino.h>
#include "../../include/Config.h"
#include "../../include/DeyeRegisters.h"
#include "RtuMaster.h"
#include "../core/RegisterCache.h"
#include "../core/WriteQueue.h"

// Periodically reads the Deye register blocks over Modbus RTU and refreshes
// the register cache. Pending writes are applied between block reads, so
// write bursts from evcc or MQTT never collide with a poll response.
namespace DeyeBridge
{
    class DeyePoller
    {
    public:
        void Init(Rtu::RtuMaster *master, RegisterCache *cache, WriteQueue *queue,
                  uint8_t slaveId, uint32_t pollIntervalMs);

        // Call from the main loop; internally paced by the poll interval
        void Run();

        uint32_t LastSweepMs() const { return mLastSweepMs; }
        uint32_t SweepCount() const { return mSweepCount; }
        uint32_t ErrorCount() const { return mErrorCount; }

    private:
        bool ReadBlock(uint16_t start, uint16_t count);
        void DrainWriteQueue();

        Rtu::RtuMaster *mMaster = nullptr;
        RegisterCache *mCache = nullptr;
        WriteQueue *mQueue = nullptr;
        uint8_t mSlaveId = 1;
        uint32_t mPollIntervalMs = 2000;

        uint32_t mNextRunMs = 0;
        size_t mBlockIndex = 0;
        uint32_t mCycle = 0;

        uint32_t mLastSweepMs = 0;
        uint32_t mSweepCount = 0;
        uint32_t mErrorCount = 0;
    };
}
