#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>

// Persistent configuration, stored in NVS. All fields may be edited in the
// web interface; the device reboots after saving.
namespace DeyeBridge
{
    struct Settings
    {
        char wifiSsid[33] = "";
        char wifiPassword[65] = "";
        char mqttHost[64] = "";
        uint16_t mqttPort = 1883;
        char mqttUser[33] = "";
        char mqttPassword[65] = "";
        char mqttClientId[33] = "deye-bridge";
        char mqttBaseTopic[33] = "deye";
        uint32_t pollIntervalMs = 2000;
        uint32_t publishIntervalMs = 5000;
        uint8_t slaveId = 1;
        uint32_t rtuBaud = 9600;
        int rtuTxPin = 12;
        int rtuRxPin = 11;
        int rtuDePin = 18;
    };

    class SettingsStore
    {
    public:
        void Begin();
        void Load();
        bool Save();   // returns false if NVS write failed

        Settings &Get() { return mSettings; }

    private:
        Preferences mPrefs;
        Settings mSettings;
    };
}
