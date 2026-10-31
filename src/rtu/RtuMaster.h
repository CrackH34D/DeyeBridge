#pragma once

#include <Arduino.h>
#include <stdint.h>

// Minimal Modbus RTU master over a HardwareSerial with DE/RE direction pin.
// Implements exactly the function codes needed here: FC03 (read holding),
// FC06 (write single), FC16 (write multiple).

namespace Rtu
{
    class RtuMaster
    {
    public:
        void Init(int uartNum, int txPin, int rxPin, int dePin, uint32_t baud);

        // Blocking, bounded by responseTimeoutMs. Returns false on timeout,
        // CRC error, exception response or malformed frame.
        bool ReadHolding(uint8_t slave, uint16_t start, uint16_t count,
                         uint16_t *out, uint32_t responseTimeoutMs);
        bool WriteSingle(uint8_t slave, uint16_t reg, uint16_t value,
                         uint32_t responseTimeoutMs);
        bool WriteMultiple(uint8_t slave, uint16_t start, const uint16_t *values,
                           uint16_t count, uint32_t responseTimeoutMs);

        // Error-class counters for remote diagnostics
        uint32_t TimeoutCount() const { return mTimeoutCount; }
        uint32_t CrcErrorCount() const { return mCrcErrorCount; }
        uint32_t ExceptionCount() const { return mExceptionCount; }

    private:
        enum class RxStatus
        {
            kOk,
            kTimeout,
            kCrc
        };

        void SendFrame(const uint8_t *frame, size_t length);
        // Reads a full response frame into frame/length. Returns the failure
        // class so callers can count timeouts and CRC errors separately.
        RxStatus ReceiveFrame(uint8_t *frame, size_t &length, size_t expectedMax,
                              uint32_t responseTimeoutMs);

        HardwareSerial *mSerial = nullptr;
        int mDePin = -1;   // <= 0: auto-direction transceiver, no pin driven
        uint32_t mTimeoutCount = 0;
        uint32_t mCrcErrorCount = 0;
        uint32_t mExceptionCount = 0;
    };
}
