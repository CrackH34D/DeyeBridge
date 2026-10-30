#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "Settings.h"
#include "../core/RegisterCache.h"

// Minimal configuration web interface. Serves on port 80 in both STA and AP
// mode. Endpoints: / (config form), /save (POST), /values (JSON status).
namespace DeyeBridge
{
    class WebConfig
    {
    public:
        void Init(SettingsStore *store, RegisterCache *cache);

        // Call from the main loop: accepts and serves HTTP requests
        void ProcessEvents();

    private:
        void ServeClient(WiFiClient &client);
        void ServeConfigPage(WiFiClient &client);
        void ServeValues(WiFiClient &client);
        void HandleSave(WiFiClient &client, const char *body);
        void HandleOtaUpload(WiFiClient &client, size_t contentLength);
        void SendResponse(WiFiClient &client, const char *contentType, const char *body, bool close);

        SettingsStore *mStore = nullptr;
        RegisterCache *mCache = nullptr;
        WiFiServer *mServer = nullptr;
    };
}
