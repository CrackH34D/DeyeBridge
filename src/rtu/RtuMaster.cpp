#include "RtuMaster.h"
#include "Crc16.h"

namespace Rtu
{
    void RtuMaster::Init(int uartNum, int txPin, int rxPin, int dePin, uint32_t baud)
    {
        mSerial = (uartNum == 1) ? &Serial1 : &Serial2;
        mDePin = dePin;

        // dePin <= 0 means auto-direction transceiver: no pin is driven
        if (mDePin > 0)
        {
            pinMode(mDePin, OUTPUT);
            digitalWrite(mDePin, LOW);   // receive mode
        }

        mSerial->begin(baud, SERIAL_8N1, rxPin, txPin);
        // Drain any startup noise on the line
        while (mSerial->available())
        {
            mSerial->read();
        }
    }

    void RtuMaster::SendFrame(const uint8_t *frame, size_t length)
    {
        if (mDePin > 0)
        {
            digitalWrite(mDePin, HIGH);
            delayMicroseconds(100);   // let the transceiver settle
        }
        mSerial->write(frame, length);
        mSerial->flush();             // wait until fully shifted out
        if (mDePin > 0)
        {
            digitalWrite(mDePin, LOW);
            delayMicroseconds(100);
        }
    }

    RtuMaster::RxStatus RtuMaster::ReceiveFrame(uint8_t *frame, size_t &length, size_t expectedMax,
                                                uint32_t responseTimeoutMs)
    {
        const uint32_t deadline = millis() + responseTimeoutMs;
        size_t received = 0;

        while (received < expectedMax && millis() < deadline)
        {
            while (mSerial->available() && received < expectedMax)
            {
                frame[received++] = static_cast<uint8_t>(mSerial->read());
            }
            if (received > 0 && !mSerial->available())
            {
                // Inter-frame gap heuristic: give the slave a short window to
                // finish the frame, then treat silence as end of frame
                const uint32_t gapDeadline = millis() + 10;
                while (millis() < gapDeadline && !mSerial->available())
                {
                    delay(1);
                }
                if (!mSerial->available())
                {
                    break;
                }
            }
            if (received < expectedMax)
            {
                delay(1);
            }
        }

        if (received < 4)
        {
            ++mTimeoutCount;
            return RxStatus::kTimeout;
        }

        const uint16_t crc = Crc16(frame, received - 2);
        const uint16_t receivedCrc = static_cast<uint16_t>(frame[received - 2]) |
                                     (static_cast<uint16_t>(frame[received - 1]) << 8);
        if (crc != receivedCrc)
        {
            ++mCrcErrorCount;
            return RxStatus::kCrc;
        }

        length = received;
        return RxStatus::kOk;
    }

    bool RtuMaster::ReadHolding(uint8_t slave, uint16_t start, uint16_t count,
                                uint16_t *out, uint32_t responseTimeoutMs)
    {
        uint8_t frame[8];
        frame[0] = slave;
        frame[1] = 0x03;
        frame[2] = static_cast<uint8_t>(start >> 8);
        frame[3] = static_cast<uint8_t>(start & 0xFF);
        frame[4] = static_cast<uint8_t>(count >> 8);
        frame[5] = static_cast<uint8_t>(count & 0xFF);
        const uint16_t crc = Crc16(frame, 6);
        frame[6] = static_cast<uint8_t>(crc & 0xFF);
        frame[7] = static_cast<uint8_t>(crc >> 8);

        SendFrame(frame, 8);

        uint8_t response[3 + 125 * 2 + 2];
        size_t length = 0;
        const size_t expectedMax = 5 + count * 2;
        if (ReceiveFrame(response, length, expectedMax, responseTimeoutMs) != RxStatus::kOk)
        {
            return false;
        }

        if (response[0] != slave)
        {
            return false;
        }
        if (response[1] == (0x03 | 0x80))
        {
            ++mExceptionCount;
            return false;   // exception response
        }
        if (response[1] != 0x03 || length != expectedMax ||
            response[2] != count * 2)
        {
            return false;
        }

        for (uint16_t i = 0; i < count; ++i)
        {
            out[i] = (static_cast<uint16_t>(response[3 + i * 2]) << 8) |
                     static_cast<uint16_t>(response[4 + i * 2]);
        }
        return true;
    }

    bool RtuMaster::WriteSingle(uint8_t slave, uint16_t reg, uint16_t value,
                                uint32_t responseTimeoutMs)
    {
        // The Deye commits settings writes only via FC16 (the proven ESPHome
        // config used use_write_multiple everywhere); FC06 is echoed but the
        // internal settings management reverts the value seconds later
        return WriteMultiple(slave, reg, &value, 1, responseTimeoutMs);
    }

    bool RtuMaster::WriteMultiple(uint8_t slave, uint16_t start, const uint16_t *values,
                                  uint16_t count, uint32_t responseTimeoutMs)
    {
        if (count == 0 || count > 120)
        {
            return false;
        }

        uint8_t frame[9 + 120 * 2];
        const size_t frameLength = 9 + count * 2;
        frame[0] = slave;
        frame[1] = 0x10;
        frame[2] = static_cast<uint8_t>(start >> 8);
        frame[3] = static_cast<uint8_t>(start & 0xFF);
        frame[4] = static_cast<uint8_t>(count >> 8);
        frame[5] = static_cast<uint8_t>(count & 0xFF);
        frame[6] = static_cast<uint8_t>(count * 2);
        for (uint16_t i = 0; i < count; ++i)
        {
            frame[7 + i * 2] = static_cast<uint8_t>(values[i] >> 8);
            frame[8 + i * 2] = static_cast<uint8_t>(values[i] & 0xFF);
        }
        const uint16_t crc = Crc16(frame, frameLength - 2);
        frame[frameLength - 2] = static_cast<uint8_t>(crc & 0xFF);
        frame[frameLength - 1] = static_cast<uint8_t>(crc >> 8);

        SendFrame(frame, frameLength);

        uint8_t response[8];
        size_t length = 0;
        if (ReceiveFrame(response, length, 8, responseTimeoutMs) != RxStatus::kOk)
        {
            return false;
        }
        if (length == 8 && response[1] == (0x10 | 0x80))
        {
            ++mExceptionCount;
            return false;
        }
        return length == 8 && response[0] == slave && response[1] == 0x10 &&
               response[2] == frame[2] && response[3] == frame[3] &&
               response[4] == frame[4] && response[5] == frame[5];
    }
}
