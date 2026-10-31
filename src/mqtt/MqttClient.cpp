#include "MqttClient.h"
#include "../../include/Config.h"
#include "../util/Logging.h"
#include "../core/ValueFormat.h"

namespace DeyeBridge
{
    void MqttClient::Init(WiFiClient &wifi, RegisterCache *cache, WriteQueue *queue,
                          const Settings &settings)
    {
        mWifi = &wifi;
        mCache = cache;
        mQueue = queue;
        mSettings = &settings;

        snprintf(mBase, sizeof(mBase), "%s/", settings.mqttBaseTopic);

        mClient.setClient(*mWifi);
        mClient.setServer(settings.mqttHost, settings.mqttPort);
        mClient.setBufferSize(768);
        mClient.setKeepAlive(30);
        mClient.setCallback([this](char *topic, byte *payload, unsigned int length) {
            HandleCommand(topic, payload, length);
        });

        mNextReconnectMs = 0;
    }

    void MqttClient::Topic(char *out, size_t size, const char *suffix) const
    {
        snprintf(out, size, "%s%s", mBase, suffix);
    }

    void MqttClient::SendIfConnected(const char *topic, const char *payload, bool retain)
    {
        if (!mClient.publish(topic, payload, retain))
        {
            Log::warn("MQTT: publish failed on %s", topic);
        }
    }

    void MqttClient::Connect()
    {
        char statusTopic[64];
        Topic(statusTopic, sizeof(statusTopic), "status");

        Log::info("MQTT: connecting to %s:%u as %s", mSettings->mqttHost,
                 mSettings->mqttPort, mSettings->mqttClientId);

        const bool ok = mClient.connect(
            mSettings->mqttClientId,
            mSettings->mqttUser[0] ? mSettings->mqttUser : nullptr,
            mSettings->mqttPassword[0] ? mSettings->mqttPassword : nullptr,
            statusTopic, 1, true, "offline");

        if (!ok)
        {
            Log::warn("MQTT: connect failed, state %d", mClient.state());
            return;
        }

        Log::info("MQTT: connected");
        mDiscoverySent = false;

        char setTopic[64];
        snprintf(setTopic, sizeof(setTopic), "%sset/+", mBase);
        mClient.subscribe(setTopic, 1);

        SendIfConnected(statusTopic, "online", true);
    }

    void MqttClient::ProcessEvents()
    {
        if (mSettings->mqttHost[0] == '\0')
        {
            return;
        }

        if (!mClient.connected())
        {
            const uint32_t now = millis();
            if (now >= mNextReconnectMs)
            {
                mNextReconnectMs = now + Config::MqttReconnectIntervalMs;
                Connect();
            }
            return;
        }

        mClient.loop();

        const uint32_t now = millis();
        if (!mDiscoverySent)
        {
            AdvertiseDiscovery();
            mDiscoverySent = true;
            mNextPublishMs = 0;   // publish values immediately after discovery
        }
        if (now >= mNextPublishMs)
        {
            mNextPublishMs = now + mSettings->publishIntervalMs;
            PublishValues();
        }
    }

    void MqttClient::AdvertiseDiscovery()
    {
        char topic[96];
        char payload[700];
        const char *device =
            "\"device\":{\"identifiers\":[\"deye-bridge\"],\"name\":\"Deye\","
            "\"manufacturer\":\"Deye\",\"model\":\"Hybrid Inverter (DeyeBridge)\",\"sw_version\":\"" FIRMWARE_VERSION "\"}";

        // Numeric sensors
        for (size_t i = 0; i < DeyeRegisters::kSensorCount; ++i)
        {
            const DeyeRegisters::SensorEntity &e = DeyeRegisters::kSensors[i];
            char stateTopic[72];
            char defId[56];
            snprintf(stateTopic, sizeof(stateTopic), "%ssensor/%s", mBase, e.key);
            snprintf(defId, sizeof(defId), "sensor.deye_%s", e.key);

            int n = snprintf(payload, sizeof(payload),
                "{\"def_ent_id\":\"%s\",\"name\":\"%s\",\"unique_id\":\"deye-bridge_%s\","
                "\"state_topic\":\"%s\","
                "\"availability_topic\":\"%sstatus\",",
                defId, e.name, e.key, stateTopic, mBase);
            if (e.unit)
            {
                n += snprintf(payload + n, sizeof(payload) - n, "\"unit_of_measurement\":\"%s\",", e.unit);
            }
            if (e.deviceClass)
            {
                n += snprintf(payload + n, sizeof(payload) - n, "\"device_class\":\"%s\",", e.deviceClass);
            }
            if (e.stateClass)
            {
                n += snprintf(payload + n, sizeof(payload) - n, "\"state_class\":\"%s\",", e.stateClass);
            }
            snprintf(payload + n, sizeof(payload) - n, "%s}", device);

            snprintf(topic, sizeof(topic), "homeassistant/sensor/deye-bridge/%s/config", e.key);
            SendIfConnected(topic, payload, true);
        }

        // Binary sensors
        for (size_t i = 0; i < DeyeRegisters::kBinaryCount; ++i)
        {
            const DeyeRegisters::BinaryEntity &e = DeyeRegisters::kBinaries[i];
            char stateTopic[72];
            char defId[56];
            snprintf(stateTopic, sizeof(stateTopic), "%ssensor/%s", mBase, e.key);
            snprintf(defId, sizeof(defId), "binary_sensor.deye_%s", e.key);
            snprintf(payload, sizeof(payload),
                "{\"def_ent_id\":\"%s\",\"name\":\"%s\",\"unique_id\":\"deye-bridge_%s\","
                "\"state_topic\":\"%s\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
                "\"availability_topic\":\"%sstatus\",%s}",
                defId, e.name, e.key, stateTopic, mBase, device);
            snprintf(topic, sizeof(topic), "homeassistant/binary_sensor/deye-bridge/%s/config", e.key);
            SendIfConnected(topic, payload, true);
        }

        // Text sensors: overall state, computed time slots
        {
            char stateTopic[72];
            snprintf(stateTopic, sizeof(stateTopic), "%ssensor/overall_state", mBase);
            snprintf(payload, sizeof(payload),
                "{\"def_ent_id\":\"sensor.deye_overall_state\","
                "\"name\":\"Overall State\",\"unique_id\":\"deye-bridge_overall_state\","
                "\"state_topic\":\"%s\",\"availability_topic\":\"%sstatus\",%s}",
                stateTopic, mBase, device);
            snprintf(topic, sizeof(topic), "homeassistant/sensor/deye-bridge/overall_state/config");
            SendIfConnected(topic, payload, true);
        }
        for (size_t i = 0; i < DeyeRegisters::kTimeSlotCount; ++i)
        {
            const DeyeRegisters::TextEntity &e = DeyeRegisters::kTimeSlots[i];
            char stateTopic[72];
            char defId[56];
            snprintf(stateTopic, sizeof(stateTopic), "%ssensor/%s", mBase, e.key);
            snprintf(defId, sizeof(defId), "sensor.deye_%s", e.key);
            snprintf(payload, sizeof(payload),
                "{\"def_ent_id\":\"%s\",\"name\":\"%s\",\"unique_id\":\"deye-bridge_%s\","
                "\"state_topic\":\"%s\",\"icon\":\"mdi:clock\","
                "\"availability_topic\":\"%sstatus\",%s}",
                defId, e.name, e.key, stateTopic, mBase, device);
            snprintf(topic, sizeof(topic), "homeassistant/sensor/deye-bridge/%s/config", e.key);
            SendIfConnected(topic, payload, true);
        }

        // Computed total PV power (legacy name "Solar Power" kept for entity ids)
        {
            char stateTopic[72];
            snprintf(stateTopic, sizeof(stateTopic), "%ssensor/solar_power", mBase);
            snprintf(payload, sizeof(payload),
                "{\"def_ent_id\":\"sensor.deye_solar_power\","
                "\"name\":\"Solar Power\",\"unique_id\":\"deye-bridge_solar_power\","
                "\"state_topic\":\"%s\",\"availability_topic\":\"%sstatus\","
                "\"unit_of_measurement\":\"W\",\"device_class\":\"power\","
                "\"state_class\":\"measurement\",%s}",
                stateTopic, mBase, device);
            snprintf(topic, sizeof(topic), "homeassistant/sensor/deye-bridge/solar_power/config");
            SendIfConnected(topic, payload, true);
        }

        // Controls: numbers, switches, selects
        for (size_t i = 0; i < DeyeRegisters::kControlCount; ++i)
        {
            const DeyeRegisters::ControlEntity &e = DeyeRegisters::kControls[i];
            const char *domain = e.isSelect ? "select" : (e.isSwitch ? "switch" : "number");
            char stateTopic[72];
            char commandTopic[72];
            char defId[56];
            snprintf(stateTopic, sizeof(stateTopic), "%ssensor/%s", mBase, e.key);
            snprintf(commandTopic, sizeof(commandTopic), "%sset/%s", mBase, e.key);
            snprintf(defId, sizeof(defId), "%s.deye_%s", domain, e.key);

            int n;
            if (e.isSwitch)
            {
                n = snprintf(payload, sizeof(payload),
                    "{\"def_ent_id\":\"%s\",\"name\":\"%s\",\"unique_id\":\"deye-bridge_%s\","
                    "\"state_topic\":\"%s\",\"command_topic\":\"%s\","
                    "\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
                    "\"availability_topic\":\"%sstatus\",",
                    defId, e.name, e.key, stateTopic, commandTopic, mBase);
            }
            else if (e.isSelect)
            {
                n = snprintf(payload, sizeof(payload),
                    "{\"def_ent_id\":\"%s\",\"name\":\"%s\",\"unique_id\":\"deye-bridge_%s\","
                    "\"state_topic\":\"%s\",\"command_topic\":\"%s\","
                    "\"options\":[",
                    defId, e.name, e.key, stateTopic, commandTopic);
                for (size_t o = 0; o < e.optionCount; ++o)
                {
                    n += snprintf(payload + n, sizeof(payload) - n, "%s\"%s\"",
                                  o ? "," : "", e.options[o].label);
                }
                n += snprintf(payload + n, sizeof(payload) - n, "],"
                    "\"availability_topic\":\"%sstatus\",", mBase);
            }
            else
            {
                n = snprintf(payload, sizeof(payload),
                    "{\"def_ent_id\":\"%s\",\"name\":\"%s\",\"unique_id\":\"deye-bridge_%s\","
                    "\"state_topic\":\"%s\",\"command_topic\":\"%s\","
                    "\"min\":%.0f,\"max\":%.0f,\"step\":%.0f,\"mode\":\"box\","
                    "\"availability_topic\":\"%sstatus\",",
                    defId, e.name, e.key, stateTopic, commandTopic,
                    e.minValue, e.maxValue, e.step, mBase);
                if (e.unit)
                {
                    n += snprintf(payload + n, sizeof(payload) - n, "\"unit_of_measurement\":\"%s\",", e.unit);
                }
            }
            if (e.icon)
            {
                n += snprintf(payload + n, sizeof(payload) - n, "\"icon\":\"%s\",", e.icon);
            }
            snprintf(payload + n, sizeof(payload) - n, "%s}", device);

            snprintf(topic, sizeof(topic), "homeassistant/%s/deye-bridge/%s/config", domain, e.key);
            SendIfConnected(topic, payload, true);
        }

        Log::info("MQTT: Home Assistant discovery published (%u sensors, %u binaries, %u controls)",
                  static_cast<unsigned>(DeyeRegisters::kSensorCount + 2 + DeyeRegisters::kTimeSlotCount),
                  static_cast<unsigned>(DeyeRegisters::kBinaryCount),
                  static_cast<unsigned>(DeyeRegisters::kControlCount));

        // Clear retained discovery topics left behind by entity renames, so no
        // ghost entities survive on the broker
        const char *legacyTopics[] = {
            "homeassistant/sensor/deye-bridge/pv_total_power/config",
            "homeassistant/sensor/deye-bridge/ct_power/config",
            "homeassistant/binary_sensor/deye-bridge/grid_connected/config",
            "homeassistant/switch/deye-bridge/solar_sell/config",
            "homeassistant/switch/deye-bridge/time_of_use/config",
            "homeassistant/switch/deye-bridge/grid_charge_time1/config",
            "homeassistant/switch/deye-bridge/grid_charge_time2/config",
            "homeassistant/switch/deye-bridge/grid_charge_time3/config",
            "homeassistant/switch/deye-bridge/grid_charge_time4/config",
            "homeassistant/switch/deye-bridge/grid_charge_time5/config",
            "homeassistant/switch/deye-bridge/grid_charge_time6/config",
            "homeassistant/number/deye-bridge/grid_charge_current/config",
        };
        for (size_t i = 0; i < sizeof(legacyTopics) / sizeof(legacyTopics[0]); ++i)
        {
            SendIfConnected(legacyTopics[i], "", true);
        }
    }

    void MqttClient::FormatValue(const DeyeRegisters::SensorEntity &entity, char *out, size_t size) const
    {
        const float value = EntityValue(entity, *mCache);
        snprintf(out, size, "%.*f", EntityDecimals(entity), value);
    }

    void MqttClient::PublishValues()
    {
        char topic[72];
        char value[24];

        for (size_t i = 0; i < DeyeRegisters::kSensorCount; ++i)
        {
            const DeyeRegisters::SensorEntity &e = DeyeRegisters::kSensors[i];
            if (!mCache->IsValid(e.address))
            {
                continue;
            }
            FormatValue(e, value, sizeof(value));
            snprintf(topic, sizeof(topic), "%ssensor/%s", mBase, e.key);
            SendIfConnected(topic, value, false);
        }

        // Binary sensors
        for (size_t i = 0; i < DeyeRegisters::kBinaryCount; ++i)
        {
            const DeyeRegisters::BinaryEntity &e = DeyeRegisters::kBinaries[i];
            if (!mCache->IsValid(e.address))
            {
                continue;
            }
            snprintf(topic, sizeof(topic), "%ssensor/%s", mBase, e.key);
            SendIfConnected(topic, mCache->Get(e.address) != 0 ? "ON" : "OFF", false);
        }

        // Overall state as text
        if (mCache->IsValid(500))
        {
            snprintf(topic, sizeof(topic), "%ssensor/overall_state", mBase);
            SendIfConnected(topic, DeyeRegisters::OverallStateName(mCache->Get(500)), false);
        }

        // Computed time slots (HH:MM from raw HHMM)
        for (size_t i = 0; i < DeyeRegisters::kTimeSlotCount; ++i)
        {
            const DeyeRegisters::TextEntity &e = DeyeRegisters::kTimeSlots[i];
            if (!mCache->IsValid(e.address))
            {
                continue;
            }
            const uint16_t raw = mCache->Get(e.address);
            snprintf(value, sizeof(value), "%02u:%02u",
                     static_cast<unsigned>(raw / 100), static_cast<unsigned>(raw % 100));
            snprintf(topic, sizeof(topic), "%ssensor/%s", mBase, e.key);
            SendIfConnected(topic, value, false);
        }

        // Computed total PV power (raw register values, W)
        if (mCache->IsValid(672))
        {
            const int32_t total = static_cast<int32_t>(mCache->Get(672)) +
                                  mCache->Get(673) + mCache->Get(674) + mCache->Get(675);
            snprintf(value, sizeof(value), "%ld", static_cast<long>(total));
            snprintf(topic, sizeof(topic), "%ssensor/solar_power", mBase);
            SendIfConnected(topic, value, false);
        }

        // Control entity states
        for (size_t i = 0; i < DeyeRegisters::kControlCount; ++i)
        {
            const DeyeRegisters::ControlEntity &e = DeyeRegisters::kControls[i];
            if (!mCache->IsValid(e.address))
            {
                continue;
            }
            snprintf(topic, sizeof(topic), "%ssensor/%s", mBase, e.key);
            const uint16_t raw = mCache->Get(e.address);

            if (e.isSwitch)
            {
                const bool on = (raw & e.clearMask) == (e.onValue & e.clearMask);
                SendIfConnected(topic, on ? "ON" : "OFF", false);
            }
            else if (e.isSelect)
            {
                const char *label = "unknown";
                for (size_t o = 0; o < e.optionCount; ++o)
                {
                    if (e.options[o].value == raw)
                    {
                        label = e.options[o].label;
                        break;
                    }
                }
                SendIfConnected(topic, label, false);
            }
            else
            {
                const float engineeringValue = (e.isSigned
                    ? static_cast<float>(static_cast<int16_t>(raw))
                    : static_cast<float>(raw)) * e.scale;
                snprintf(value, sizeof(value), "%.0f", engineeringValue);
                SendIfConnected(topic, value, false);
            }
        }

        // Compact JSON state for non-HA consumers
        static char json[4096];
        BuildStateJson(*mCache, json, sizeof(json));

        snprintf(topic, sizeof(topic), "%sstate", mBase);
        SendIfConnected(topic, json, false);
    }

    void MqttClient::HandleCommand(char *topic, byte *payload, unsigned int length)
    {
        // Topic layout: <base>set/<key>
        const size_t baseLen = strlen(mBase);
        if (strncmp(topic, mBase, baseLen) != 0 ||
            strncmp(topic + baseLen, "set/", 4) != 0)
        {
            return;
        }
        const char *keyStart = topic + baseLen + 4;

        char key[40];
        size_t keyLength = strlen(keyStart);
        if (keyLength >= sizeof(key))
        {
            keyLength = sizeof(key) - 1;
        }
        memcpy(key, keyStart, keyLength);
        key[keyLength] = '\0';

        char payloadText[48];
        const size_t copyLength = length < sizeof(payloadText) - 1 ? length : sizeof(payloadText) - 1;
        memcpy(payloadText, payload, copyLength);
        payloadText[copyLength] = '\0';

        for (size_t i = 0; i < DeyeRegisters::kControlCount; ++i)
        {
            const DeyeRegisters::ControlEntity &e = DeyeRegisters::kControls[i];
            if (strcmp(key, e.key) != 0)
            {
                continue;
            }

            uint16_t raw;
            if (e.isSwitch)
            {
                const bool on = strcmp(payloadText, "ON") == 0 || strcmp(payloadText, "on") == 0 ||
                                strcmp(payloadText, "1") == 0 || strcmp(payloadText, "true") == 0;
                raw = on ? e.onValue : e.offValue;
            }
            else if (e.isSelect)
            {
                bool matched = false;
                raw = 0;
                for (size_t o = 0; o < e.optionCount; ++o)
                {
                    if (strcmp(payloadText, e.options[o].label) == 0)
                    {
                        raw = e.options[o].value;
                        matched = true;
                        break;
                    }
                }
                if (!matched)
                {
                    Log::warn("MQTT: unknown option '%s' for %s", payloadText, e.key);
                    return;
                }
            }
            else
            {
                const float value = atof(payloadText);
                const float clamped = value < e.minValue ? e.minValue : (value > e.maxValue ? e.maxValue : value);
                const int32_t scaled = static_cast<int32_t>(clamped / e.scale + (clamped < 0 ? -0.5f : 0.5f));
                raw = static_cast<uint16_t>(scaled);
            }

            if (!DeyeRegisters::IsWritable(e.address))
            {
                Log::warn("MQTT: register %u for %s is not writable", e.address, e.key);
                return;
            }
            if (mQueue->Push(e.address, raw, e.clearMask))
            {
                // Optimistic mirror, masked like the poller will apply it
                mCache->Poke(e.address,
                             static_cast<uint16_t>((mCache->Get(e.address) &
                                                    static_cast<uint16_t>(~e.clearMask)) |
                                                   (raw & e.clearMask)));
                Log::info("MQTT: queued write %s (reg %u) = %u (mask %04X)",
                          e.key, e.address, raw, e.clearMask);
            }
            else
            {
                Log::warn("MQTT: write queue full, dropped command for %s", e.key);
            }
            return;
        }

        Log::warn("MQTT: unknown command key '%s'", key);
    }

    void MqttClient::PublishDiscovery()
    {
        // Public helper: re-publish discovery after config changes (debug)
        if (mClient.connected())
        {
            AdvertiseDiscovery();
        }
    }
}
