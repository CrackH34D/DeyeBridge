#pragma once

// Firmware version, baked into MQTT discovery (sw_version) and the web footer.
// Bump on every release; tools/deploy.sh reads it from here.
#define FIRMWARE_VERSION "1.4.2"

namespace Config
{
    // RS485 / Modbus RTU (Deye hybrid inverter, slave 1, 9600 8N1)
    constexpr int RtuUartNum = 2;
    constexpr int RtuTxPin = 12;
    constexpr int RtuRxPin = 11;
    constexpr int RtuDePin = 18;   // DE/RE of the MAX485, HIGH = transmit
    constexpr uint32_t RtuBaud = 9600;
    constexpr uint8_t DeyeSlaveId = 1;

    // Deye holding registers are 0..~800; the cache covers the whole window
    constexpr uint16_t RegisterSpaceEnd = 1000;

    // Modbus TCP server for evcc
    constexpr uint16_t TcpPort = 502;
    constexpr uint8_t TcpMaxClients = 3;
    constexpr uint8_t TcpUnitId = 1;   // slave id presented on TCP (pass-through target)

    // Timeouts
    constexpr uint32_t RtuResponseTimeoutMs = 250;
    constexpr uint32_t TcpIdleTimeoutMs = 120000;

    // Loops
    constexpr uint32_t PollIntervalMs = 2000;      // hot blocks cadence
    constexpr uint32_t MqttPublishIntervalMs = 5000;
    constexpr uint32_t MqttReconnectIntervalMs = 5000;

    // Watchdog: main loop must notify at least this often
    constexpr uint32_t WatchdogTimeoutMs = 20000;

    // Fallback AP while unconfigured
    constexpr const char *ApSsid = "DeyeBridge-Setup";
    constexpr const char *ApPassword = "deye12345";
}
