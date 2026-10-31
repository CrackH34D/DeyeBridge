#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "Settings.h"
#include "../core/RegisterCache.h"
#include "../rtu/RtuMaster.h"

// Minimal configuration web interface. Serves on port 80 in both STA and AP
// mode. Endpoints: / (config form), /save (POST), /values (JSON status),
// /scan (WiFi scan JSON), /diag (runtime diagnostics), /update (POST, OTA).
namespace DeyeBridge
{
    class MqttClient;
    class DeyePoller;

    class WebConfig
    {
    public:
        void Init(SettingsStore *store, RegisterCache *cache);

        // Optional pointers for the /diag endpoint; safe to skip
        void AttachDiagnostics(const MqttClient *mqtt, const DeyePoller *poller,
                               const Rtu::RtuMaster *rtu);

        // Call from the main loop: accepts and serves HTTP requests
        void ProcessEvents();

    private:
        void ServeClient(WiFiClient &client);
        void ServeConfigPage(WiFiClient &client);
        void ServeValues(WiFiClient &client);
        void ServeScan(WiFiClient &client);
        void ServeDiag(WiFiClient &client);
        void HandleSave(WiFiClient &client, const char *body);
        void HandleOtaUpload(WiFiClient &client, size_t contentLength);
        void SendResponse(WiFiClient &client, const char *contentType, const char *body);

        SettingsStore *mStore = nullptr;
        RegisterCache *mCache = nullptr;
        WiFiServer *mServer = nullptr;
        const MqttClient *mMqtt = nullptr;
        const DeyePoller *mPoller = nullptr;
        const Rtu::RtuMaster *mRtu = nullptr;
    };
}
