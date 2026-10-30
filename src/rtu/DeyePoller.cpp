#include "DeyePoller.h"

using namespace DeyeRegisters;

namespace DeyeBridge
{
    void DeyePoller::Init(Rtu::RtuMaster *master, RegisterCache *cache, WriteQueue *queue,
                          uint8_t slaveId, uint32_t pollIntervalMs)
    {
        mMaster = master;
        mCache = cache;
        mQueue = queue;
        mSlaveId = slaveId;
        mPollIntervalMs = pollIntervalMs;
    }

    bool DeyePoller::ReadBlock(uint16_t start, uint16_t count)
    {
        uint16_t values[125];
        if (!mMaster->ReadHolding(mSlaveId, start, count, values, Config::RtuResponseTimeoutMs))
        {
            ++mErrorCount;
            return false;
        }
        mCache->Store(start, values, count);
        return true;
    }

    void DeyePoller::DrainWriteQueue()
    {
        // Apply at most a few writes per cycle; the rest wait for the next
        // round so a write burst cannot starve polling completely
        for (int applied = 0; applied < 4 && !mQueue->Empty(); ++applied)
        {
            WriteEntry entry;
            if (!mQueue->Pop(entry))
            {
                break;
            }

            // Single-word writes go out as FC06; bursts of adjacent registers
            // are coalesced later, one frame per entry keeps the queue simple
            const bool ok = mMaster->WriteSingle(mSlaveId, entry.address, entry.value,
                                                 Config::RtuResponseTimeoutMs);
            if (ok)
            {
                // Read-back instead of blind poke: refresh just this register
                uint16_t verify = 0;
                if (mMaster->ReadHolding(mSlaveId, entry.address, 1, &verify,
                                         Config::RtuResponseTimeoutMs))
                {
                    mCache->Poke(entry.address, verify);
                }
            }
            else
            {
                ++mErrorCount;
            }
        }
    }

    void DeyePoller::Run()
    {
        const uint32_t now = millis();
        if (now < mNextRunMs)
        {
            return;
        }
        mNextRunMs = now + mPollIntervalMs;
        ++mCycle;

        // Writes go first: they are the most time-critical (battery mode
        // changes) and must not queue up behind a full sweep
        DrainWriteQueue();

        bool allOk = true;
        for (size_t i = 0; i < kPollBlockCount; ++i)
        {
            const RegBlock &block = kPollBlocks[i];
            if (block.everyN > 1 && (mCycle % block.everyN) != 1)
            {
                continue;
            }
            if (!ReadBlock(block.start, block.count))
            {
                allOk = false;
                // A single failed block must not stall the sweep; continue
                // with the next one
            }
            DrainWriteQueue();
        }

        if (allOk)
        {
            mLastSweepMs = now;
            ++mSweepCount;
        }
    }
}
