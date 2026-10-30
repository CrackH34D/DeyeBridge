#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

#include "Config.h"
#include "util/Logging.h"
#include "web/Settings.h"
#include "web/WebConfig.h"
#include "rtu/RtuMaster.h"
#include "rtu/DeyePoller.h"
#include "core/RegisterCache.h"
#include "core/WriteQueue.h"
#include "tcp/ModbusTcpServer.h"
#include "mqtt/MqttClient.h"

using DeyeBridge::RegisterCache;
using DeyeBridge::SettingsStore;
using DeyeBridge::WebConfig;
using DeyeBridge::WriteQueue;
using Rtu::RtuMaster;

static SettingsStore gSettings;
static RegisterCache gCache(Config::RegisterSpaceEnd);
static WriteQueue gWriteQueue;
static RtuMaster gRtu;
static DeyeBridge::DeyePoller gPoller;
static DeyeBridge::ModbusTcpServer gTcpServer;
static DeyeBridge::MqttClient gMqtt;
static WebConfig gWeb;
static WiFiClient gWifiClientMqtt;

static TaskHandle_t gWatchdogTaskHandle = nullptr;
static bool gRtuReady = false;

// Independent watchdog task: restarts the device when the main loop stops
// notifying (WiFi/RTU/TCP/MQTT all run from the single main loop task)
static void WatchdogTask(void *)
{
    for (;;)
    {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(Config::WatchdogTimeoutMs)) == 0)
        {
            Log::error("Watchdog timeout, restarting");
            delay(100);
            esp_restart();
        }
    }
}

static bool WifiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

static void WifiStart()
{
    const DeyeBridge::Settings &s = gSettings.Get();

    if (s.wifiSsid[0])
    {
        Log::info("WiFi: connecting to '%s'", s.wifiSsid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(s.wifiSsid, s.wifiPassword);
        const uint32_t deadline = millis() + 20000;
        while (!WifiConnected() && millis() < deadline)
        {
            delay(250);
        }
    }

    if (!WifiConnected())
    {
        Log::warn("WiFi: not connected, starting fallback AP '%s'", Config::ApSsid);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(Config::ApSsid, Config::ApPassword);
    }
    else
    {
        Log::info("WiFi: connected, IP %s", WiFi.localIP().toString().c_str());
    }
}

static void WifiMaintain()
{
    static uint32_t lastAttempt = 0;
    if (WifiConnected())
    {
        return;
    }
    const uint32_t now = millis();
    if (now - lastAttempt < 30000)
    {
        return;
    }
    lastAttempt = now;

    const DeyeBridge::Settings &s = gSettings.Get();
    if (s.wifiSsid[0] == '\0')
    {
        return;   // unconfigured: stay in AP mode
    }

    Log::warn("WiFi: lost connection, reconnecting");
    WiFi.mode(WIFI_AP_STA);   // keep the config AP reachable during retries
    WiFi.begin(s.wifiSsid, s.wifiPassword);
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    Log::info("DeyeBridge booting");
    Log::info("  Modbus TCP: port %u | MQTT + HA discovery | RTU slave %u",
              Config::TcpPort, Config::DeyeSlaveId);

    gSettings.Begin();
    gSettings.Load();

    WifiStart();

    // Web interface runs in every mode (AP fallback included)
    gWeb.Init(&gSettings, &gCache);

    if (!WifiConnected())
    {
        Log::warn("No WiFi: RTU polling still runs locally, TCP serves LAN clients only");
    }

#ifndef DEYE_DUMMY_RTU
    gRtu.Init(Config::RtuUartNum, gSettings.Get().rtuTxPin, gSettings.Get().rtuRxPin,
              gSettings.Get().rtuDePin, gSettings.Get().rtuBaud);
    gRtuReady = true;
#else
    Log::warn("DUMMY_RTU: bus access disabled, using zeroed register image");
#endif

    gPoller.Init(&gRtu, &gCache, &gWriteQueue,
                 gSettings.Get().slaveId, gSettings.Get().pollIntervalMs);
    gTcpServer.Init(&gCache, &gWriteQueue);
    gMqtt.Init(gWifiClientMqtt, &gCache, &gWriteQueue, gSettings.Get());

    // Watchdog last: setup itself may block (WiFi connect)
    xTaskCreatePinnedToCore(WatchdogTask, "watchdog", 3072, nullptr, 1,
                            &gWatchdogTaskHandle, 0);

    Log::info("Setup complete");
}

void loop()
{
    if (gWatchdogTaskHandle != nullptr)
    {
        xTaskNotify(gWatchdogTaskHandle, 1, eSetValueWithOverwrite);
    }

    WifiMaintain();
    gWeb.ProcessEvents();

    if (gRtuReady)
    {
        gPoller.Run();
    }

    gTcpServer.ProcessEvents();
    gMqtt.ProcessEvents();

    delay(2);
}
