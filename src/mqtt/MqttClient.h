#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "../../include/DeyeRegisters.h"
#include "../core/RegisterCache.h"
#include "../core/WriteQueue.h"
#include "../web/Settings.h"

// MQTT client for Home Assistant: publishes sensor values, advertises the
// device via HA MQTT discovery, and accepts write commands on {base}/set/{key}
// which are pushed into the shared write queue (same path as evcc writes).
namespace DeyeBridge
{
    class MqttClient
    {
    public:
        void Init(WiFiClient &wifi, RegisterCache *cache, WriteQueue *queue,
                  const Settings &settings);

        // Call from the main loop: keeps the connection alive, publishes on
        // the configured interval and processes command messages
        void ProcessEvents();

        bool Connected() const { return const_cast<PubSubClient &>(mClient).connected(); }
        void PublishDiscovery();   // re-publish after config changes (debug)

    private:
        void Connect();
        void PublishValues();
        void AdvertiseDiscovery();
        void HandleCommand(char *topic, byte *payload, unsigned int length);
        void SendIfConnected(const char *topic, const char *payload, bool retain);

        void Topic(char *out, size_t size, const char *suffix) const;
        void FormatValue(const DeyeRegisters::SensorEntity &entity, char *out, size_t size) const;

        WiFiClient *mWifi = nullptr;
        PubSubClient mClient;
        RegisterCache *mCache = nullptr;
        WriteQueue *mQueue = nullptr;
        const Settings *mSettings = nullptr;

        char mBase[40];   // base topic with trailing slash
        uint32_t mNextPublishMs = 0;
        uint32_t mNextReconnectMs = 0;
        bool mDiscoverySent = false;
    };
}
