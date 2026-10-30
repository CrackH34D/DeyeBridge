#include "Settings.h"
#include "../util/Logging.h"

namespace DeyeBridge
{
    void SettingsStore::Begin()
    {
        if (!mPrefs.begin("deye", false))
        {
            Log::error("Settings: NVS open failed, using defaults");
        }
    }

    void SettingsStore::Load()
    {
        if (!mPrefs.begin("deye", true))
        {
            Log::warn("Settings: NVS unreadable, keeping defaults");
            return;
        }

        const String ssid = mPrefs.getString("wifi_ssid", "");
        const String pass = mPrefs.getString("wifi_pass", "");
        ssid.toCharArray(mSettings.wifiSsid, sizeof(mSettings.wifiSsid));
        pass.toCharArray(mSettings.wifiPassword, sizeof(mSettings.wifiPassword));

        const String host = mPrefs.getString("mqtt_host", "");
        host.toCharArray(mSettings.mqttHost, sizeof(mSettings.mqttHost));
        mSettings.mqttPort = mPrefs.getUShort("mqtt_port", 1883);

        const String user = mPrefs.getString("mqtt_user", "");
        user.toCharArray(mSettings.mqttUser, sizeof(mSettings.mqttUser));
        const String pw = mPrefs.getString("mqtt_pass", "");
        pw.toCharArray(mSettings.mqttPassword, sizeof(mSettings.mqttPassword));

        const String clientId = mPrefs.getString("mqtt_client", "deye-bridge");
        clientId.toCharArray(mSettings.mqttClientId, sizeof(mSettings.mqttClientId));
        const String base = mPrefs.getString("mqtt_base", "deye");
        base.toCharArray(mSettings.mqttBaseTopic, sizeof(mSettings.mqttBaseTopic));

        mSettings.pollIntervalMs = mPrefs.getULong("poll_ms", 2000);
        mSettings.publishIntervalMs = mPrefs.getULong("pub_ms", 5000);
        mSettings.slaveId = mPrefs.getUChar("slave_id", 1);
        mSettings.rtuBaud = mPrefs.getULong("rtu_baud", 9600);
        mSettings.rtuTxPin = mPrefs.getInt("rtu_tx", 12);
        mSettings.rtuRxPin = mPrefs.getInt("rtu_rx", 11);
        mSettings.rtuDePin = mPrefs.getInt("rtu_de", 18);

        mPrefs.end();
        Log::info("Settings loaded (base topic '%s', poll %u ms)",
                 mSettings.mqttBaseTopic, static_cast<unsigned>(mSettings.pollIntervalMs));
    }

    bool SettingsStore::Save()
    {
        if (!mPrefs.begin("deye", false))
        {
            return false;
        }
        mPrefs.putString("wifi_ssid", mSettings.wifiSsid);
        mPrefs.putString("wifi_pass", mSettings.wifiPassword);
        mPrefs.putString("mqtt_host", mSettings.mqttHost);
        mPrefs.putUShort("mqtt_port", mSettings.mqttPort);
        mPrefs.putString("mqtt_user", mSettings.mqttUser);
        mPrefs.putString("mqtt_pass", mSettings.mqttPassword);
        mPrefs.putString("mqtt_client", mSettings.mqttClientId);
        mPrefs.putString("mqtt_base", mSettings.mqttBaseTopic);
        mPrefs.putULong("poll_ms", mSettings.pollIntervalMs);
        mPrefs.putULong("pub_ms", mSettings.publishIntervalMs);
        mPrefs.putUChar("slave_id", mSettings.slaveId);
        mPrefs.putULong("rtu_baud", mSettings.rtuBaud);
        mPrefs.putInt("rtu_tx", mSettings.rtuTxPin);
        mPrefs.putInt("rtu_rx", mSettings.rtuRxPin);
        mPrefs.putInt("rtu_de", mSettings.rtuDePin);
        mPrefs.end();
        Log::info("Settings saved");
        return true;
    }
}
