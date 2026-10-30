#include "ModbusTcpServer.h"
#include "../../include/DeyeRegisters.h"
#include "../util/Logging.h"

namespace DeyeBridge
{
    void ModbusTcpServer::Init(RegisterCache *cache, WriteQueue *queue)
    {
        mCache = cache;
        mQueue = queue;
        mServer = new WiFiServer(Config::TcpPort);
        mServer->begin();
        mServer->setNoDelay(true);
        Log::info("Modbus TCP server listening on port %u", Config::TcpPort);
    }

    void ModbusTcpServer::DropClient(size_t index)
    {
        mClients[index].stop();
    }

    void ModbusTcpServer::ProcessEvents()
    {
        // Accept new clients, replacing the stalest one when full
        while (mServer->hasClient())
        {
            WiFiClient candidate = mServer->accept();
            if (!candidate)
            {
                continue;
            }

            int slot = -1;
            uint32_t oldest = UINT32_MAX;
            size_t oldestIndex = 0;
            for (size_t i = 0; i < Config::TcpMaxClients; ++i)
            {
                if (!mClients[i] || !mClients[i].connected())
                {
                    slot = static_cast<int>(i);
                    break;
                }
                if (mClientLastActiveMs[i] < oldest)
                {
                    oldest = mClientLastActiveMs[i];
                    oldestIndex = i;
                }
            }
            if (slot < 0)
            {
                Log::info("Modbus TCP: dropping idle client for new connection");
                DropClient(oldestIndex);
                slot = static_cast<int>(oldestIndex);
            }
            mClients[slot] = candidate;
            mClientLastActiveMs[slot] = millis();
            Log::info("Modbus TCP client connected from %s", candidate.remoteIP().toString().c_str());
        }

        // Service existing clients
        for (size_t i = 0; i < Config::TcpMaxClients; ++i)
        {
            WiFiClient &client = mClients[i];
            if (!client || !client.connected())
            {
                if (client)
                {
                    DropClient(i);
                }
                continue;
            }

            // Read complete MBAP + PDU frames while bytes are available
            while (client.available() >= 7)
            {
                uint8_t header[7];
                client.readBytes(header, 7);

                const uint16_t protocol = (static_cast<uint16_t>(header[2]) << 8) | header[3];
                const uint16_t length = (static_cast<uint16_t>(header[4]) << 8) | header[5];
                const uint16_t transactionId = (static_cast<uint16_t>(header[0]) << 8) | header[1];

                if (protocol != 0 || length < 2 || length > 1 + 253)
                {
                    Log::info("Modbus TCP: malformed MBAP, closing client");
                    DropClient(i);
                    break;
                }

                uint8_t pdu[254];
                if (static_cast<size_t>(length - 1) > sizeof(pdu) ||
                    static_cast<size_t>(client.readBytes(pdu, length - 1)) < static_cast<size_t>(length - 1))
                {
                    DropClient(i);
                    break;
                }

                uint8_t response[255];
                const size_t pduLength = HandlePdu(pdu, length - 1, response);
                if (pduLength > 0)
                {
                    uint8_t out[259];
                    out[0] = static_cast<uint8_t>(transactionId >> 8);
                    out[1] = static_cast<uint8_t>(transactionId & 0xFF);
                    out[2] = 0;   // protocol id high
                    out[3] = 0;   // protocol id low
                    out[4] = 0;   // length high
                    out[5] = static_cast<uint8_t>((pduLength + 1) & 0xFF);
                    out[6] = header[6];   // unit id echo
                    memcpy(out + 7, response, pduLength);
                    client.write(out, pduLength + 7);
                }
                mClientLastActiveMs[i] = millis();
            }

            if (millis() - mClientLastActiveMs[i] > Config::TcpIdleTimeoutMs)
            {
                Log::info("Modbus TCP: idle timeout, closing client");
                DropClient(i);
            }
        }
    }

    size_t ModbusTcpServer::BuildException(uint8_t functionCode, uint8_t exceptionCode, uint8_t *response)
    {
        response[0] = functionCode | 0x80;
        response[1] = exceptionCode;
        return 2;
    }

    size_t ModbusTcpServer::HandlePdu(const uint8_t *pdu, size_t pduLength, uint8_t *response)
    {
        if (pduLength < 1)
        {
            return 0;
        }
        switch (pdu[0])
        {
            case 0x03: return HandleReadHolding(pdu, pduLength, response);
            case 0x04: return HandleReadInput(pdu, pduLength, response);
            case 0x06: return HandleWriteSingle(pdu, pduLength, response);
            case 0x10: return HandleWriteMultiple(pdu, pduLength, response);
            default:
                Log::info("Modbus TCP: unsupported function code 0x%02X", pdu[0]);
                return BuildException(pdu[0], 0x01, response);
        }
    }

    size_t ModbusTcpServer::HandleReadHolding(const uint8_t *pdu, size_t pduLength, uint8_t *response)
    {
        if (pduLength < 5)
        {
            return 0;
        }
        const uint16_t start = (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2];
        const uint16_t count = (static_cast<uint16_t>(pdu[3]) << 8) | pdu[4];

        if (count < 1 || count > 125 ||
            start >= Config::RegisterSpaceEnd ||
            start + count > Config::RegisterSpaceEnd)
        {
            return BuildException(pdu[0], 0x02, response);
        }

        response[0] = pdu[0];
        response[1] = static_cast<uint8_t>(count * 2);
        for (uint16_t i = 0; i < count; ++i)
        {
            const uint16_t value = mCache->Get(start + i);
            response[2 + i * 2] = static_cast<uint8_t>(value >> 8);
            response[3 + i * 2] = static_cast<uint8_t>(value & 0xFF);
        }
        return 2 + count * 2;
    }

    size_t ModbusTcpServer::HandleReadInput(const uint8_t *pdu, size_t pduLength, uint8_t *response)
    {
        // The Deye map lives entirely in holding registers
        (void)pdu;
        (void)pduLength;
        return BuildException(pdu[0], 0x01, response);
    }

    size_t ModbusTcpServer::HandleWriteSingle(const uint8_t *pdu, size_t pduLength, uint8_t *response)
    {
        if (pduLength < 5)
        {
            return 0;
        }
        const uint16_t address = (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2];
        const uint16_t value = (static_cast<uint16_t>(pdu[3]) << 8) | pdu[4];

        if (!DeyeRegisters::IsWritable(address))
        {
            Log::info("Modbus TCP: write to protected register %u rejected", address);
            return BuildException(pdu[0], 0x02, response);
        }

        mQueue->Push(address, value, 0xFFFF);
        mCache->Poke(address, value);
        Log::info("Modbus TCP: queued write reg %u = %u", address, value);
        // FC06 echoes the request
        memcpy(response, pdu, 5);
        return 5;
    }

    size_t ModbusTcpServer::HandleWriteMultiple(const uint8_t *pdu, size_t pduLength, uint8_t *response)
    {
        if (pduLength < 6)
        {
            return 0;
        }
        const uint16_t start = (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2];
        const uint16_t count = (static_cast<uint16_t>(pdu[3]) << 8) | pdu[4];
        const uint16_t byteCount = pdu[5];

        if (count < 1 || count > 123 || byteCount != count * 2 ||
            pduLength < static_cast<size_t>(6 + byteCount) ||
            start >= Config::RegisterSpaceEnd ||
            start + count > Config::RegisterSpaceEnd)
        {
            return BuildException(pdu[0], 0x02, response);
        }

        for (uint16_t i = 0; i < count; ++i)
        {
            const uint16_t address = start + i;
            if (!DeyeRegisters::IsWritable(address))
            {
                Log::info("Modbus TCP: multi-write hits protected register %u, rejected", address);
                return BuildException(pdu[0], 0x02, response);
            }
        }

        for (uint16_t i = 0; i < count; ++i)
        {
            const uint16_t value = (static_cast<uint16_t>(pdu[6 + i * 2]) << 8) |
                                   static_cast<uint16_t>(pdu[7 + i * 2]);
            mQueue->Push(start + i, value, 0xFFFF);
            mCache->Poke(start + i, value);
        }
        Log::info("Modbus TCP: queued %u writes starting at reg %u", count, start);

        response[0] = pdu[0];
        response[1] = pdu[1];
        response[2] = pdu[2];
        response[3] = pdu[3];
        response[4] = pdu[4];
        return 5;
    }
}
