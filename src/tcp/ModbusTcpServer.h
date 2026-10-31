#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "../../include/Config.h"
#include "../core/RegisterCache.h"
#include "../core/WriteQueue.h"

// Minimal Modbus TCP server (MBAP + FC03/FC04/FC06/FC16) that serves the Deye
// register cache as a 1:1 pass-through. Reads answer instantly from RAM, so
// evcc's multi-register block reads never wait for an RS485 round trip.
// FC04 is answered from the same holding-register image (the Deye map has no
// separate input-register space). Writes are validated against the Deye
// register whitelist and pushed into the shared write queue.
namespace DeyeBridge
{
    class ModbusTcpServer
    {
    public:
        void Init(RegisterCache *cache, WriteQueue *queue);

        // Call from the main loop: accepts clients and services requests
        void ProcessEvents();

    private:
        // Handles one complete PDU; returns bytes written to the response or
        // 0 when the frame was invalid
        size_t HandlePdu(const uint8_t *pdu, size_t pduLength, uint8_t *response);

        size_t HandleReadHolding(const uint8_t *pdu, size_t pduLength, uint8_t *response);
        size_t HandleWriteSingle(const uint8_t *pdu, size_t pduLength, uint8_t *response);
        size_t HandleWriteMultiple(const uint8_t *pdu, size_t pduLength, uint8_t *response);

        static size_t BuildException(uint8_t functionCode, uint8_t exceptionCode, uint8_t *response);
        void DropClient(size_t index);

        WiFiServer *mServer = nullptr;
        WiFiClient mClients[Config::TcpMaxClients];
        uint32_t mClientLastActiveMs[Config::TcpMaxClients] = {};
        RegisterCache *mCache = nullptr;
        WriteQueue *mQueue = nullptr;
    };
}
