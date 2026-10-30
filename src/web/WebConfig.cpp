#include "WebConfig.h"
#include <Update.h>
#include "../util/Logging.h"
#include "../util/Watchdog.h"
#include "../core/ValueFormat.h"
#include "../../include/Config.h"

namespace DeyeBridge
{
    void WebConfig::Init(SettingsStore *store, RegisterCache *cache)
    {
        mStore = store;
        mCache = cache;
        mServer = new WiFiServer(80);
        mServer->begin();
        mServer->setNoDelay(true);
    }

    void SendFull(WiFiClient &client, const char *contentType, const char *body)
    {
        client.printf("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
                      contentType, static_cast<unsigned>(strlen(body)));
        client.print(body);
    }

    void WebConfig::SendResponse(WiFiClient &client, const char *contentType, const char *body, bool close)
    {
        (void)close;
        SendFull(client, contentType, body);
    }

    void WebConfig::ProcessEvents()
    {
        while (mServer->hasClient())
        {
            WiFiClient client = mServer->accept();
            if (client)
            {
                ServeClient(client);
                client.stop();
            }
        }
    }

    void WebConfig::ServeClient(WiFiClient &client)
    {
        client.setTimeout(2000);

        char requestLine[128];
        if (!client.readBytesUntil('\n', requestLine, sizeof(requestLine) - 1))
        {
            return;
        }
        requestLine[sizeof(requestLine) - 1] = '\0';

        // Drain headers, note content length
        size_t contentLength = 0;
        char line[128];
        for (;;)
        {
            if (!client.readBytesUntil('\n', line, sizeof(line) - 1))
            {
                break;
            }
            line[sizeof(line) - 1] = '\0';
            if (line[0] == '\r' || line[0] == '\0')
            {
                break;
            }
            if (strncasecmp(line, "Content-Length:", 15) == 0)
            {
                contentLength = static_cast<size_t>(atoi(line + 15));
            }
        }

        if (contentLength > 2048)
        {
            contentLength = 2048;
        }

        if (strncmp(requestLine, "POST /save", 10) == 0)
        {
            char body[2049] = {};
            const size_t got = client.readBytes(body, contentLength);
            body[got] = '\0';
            HandleSave(client, body);
            return;
        }

        if (strncmp(requestLine, "POST /update", 12) == 0)
        {
            HandleOtaUpload(client, contentLength);
            return;
        }

        if (strncmp(requestLine, "GET /values", 11) == 0)
        {
            ServeValues(client);
            return;
        }

        ServeConfigPage(client);
    }

    void WebConfig::HandleOtaUpload(WiFiClient &client, size_t contentLength)
    {
        Log::info("OTA: firmware upload started (%u bytes)", static_cast<unsigned>(contentLength));

        // The upload can take minutes; the watchdog must not restart us
        Watchdog::Suspend();

        const char *response;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            response = "OTA failed: not enough space";
            Log::error("OTA: Update.begin failed (%s)", Update.errorString());
        }
        else
        {
            uint8_t buffer[1024];
            size_t received = 0;
            uint32_t lastLog = millis();

            while (received < contentLength)
            {
                const size_t available = client.available();
                if (available)
                {
                    const size_t chunk = available < sizeof(buffer) ? available : sizeof(buffer);
                    const size_t got = client.readBytes(buffer, chunk);
                    if (got == 0)
                    {
                        break;
                    }
                    if (Update.write(buffer, got) != got)
                    {
                        break;
                    }
                    received += got;
                    if (millis() - lastLog > 5000)
                    {
                        lastLog = millis();
                        Log::info("OTA: %u / %u bytes", static_cast<unsigned>(received),
                                  static_cast<unsigned>(contentLength));
                    }
                }
                else if (!client.connected())
                {
                    break;
                }
                else
                {
                    delay(1);
                }
            }

            if (received == contentLength && Update.end(true))
            {
                Log::info("OTA: update complete, rebooting");
                client.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
                             "Update successful, rebooting...");
                client.flush();
                delay(500);
                ESP.restart();
                return;
            }

            response = "OTA failed: transfer incomplete or invalid image";
            Log::error("OTA: failed after %u bytes (%s)", static_cast<unsigned>(received),
                       Update.errorString());
            Update.abort();
        }

        char body[192];
        snprintf(body, sizeof(body),
                 "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n%s",
                 response);
        client.print(body);
        Watchdog::Resume();
    }

    void WebConfig::ServeValues(WiFiClient &client)
    {
        char json[1536];
        size_t n = 0;
        n += snprintf(json + n, sizeof(json) - n, "{");
        bool first = true;

        char value[24];
        for (size_t i = 0; i < DeyeRegisters::kSensorCount; ++i)
        {
            const DeyeRegisters::SensorEntity &e = DeyeRegisters::kSensors[i];
            if (!mCache->IsValid(e.address))
            {
                continue;
            }
            const float v = EntityValue(e, *mCache);
            snprintf(value, sizeof(value), "%.*f", EntityDecimals(e), v);
            n += snprintf(json + n, sizeof(json) - n, "%s\"%s\":%s", first ? "" : ",", e.key, value);
            first = false;
        }
        if (mCache->IsValid(672))
        {
            const int32_t total = static_cast<int32_t>(mCache->Get(672)) +
                                  mCache->Get(673) + mCache->Get(674) + mCache->Get(675);
            n += snprintf(json + n, sizeof(json) - n, "%s\"pv_total_power\":%ld",
                          first ? "" : ",", static_cast<long>(total));
            first = false;
        }
        if (mCache->IsValid(500))
        {
            n += snprintf(json + n, sizeof(json) - n, "%s\"overall_state\":\"%s\"",
                          first ? "" : ",", DeyeRegisters::OverallStateName(mCache->Get(500)));
            first = false;
        }
        for (size_t i = 0; i < DeyeRegisters::kBinaryCount; ++i)
        {
            const DeyeRegisters::BinaryEntity &e = DeyeRegisters::kBinaries[i];
            if (!mCache->IsValid(e.address))
            {
                continue;
            }
            n += snprintf(json + n, sizeof(json) - n, "%s\"%s\":%s",
                          first ? "" : ",", e.key,
                          mCache->Get(e.address) ? "true" : "false");
            first = false;
        }
        for (size_t i = 0; i < DeyeRegisters::kTimeSlotCount; ++i)
        {
            const DeyeRegisters::TextEntity &e = DeyeRegisters::kTimeSlots[i];
            if (!mCache->IsValid(e.address))
            {
                continue;
            }
            const uint16_t raw = mCache->Get(e.address);
            n += snprintf(json + n, sizeof(json) - n, "%s\"%s\":\"%02u:%02u\"",
                          first ? "" : ",", e.key,
                          static_cast<unsigned>(raw / 100), static_cast<unsigned>(raw % 100));
            first = false;
        }
        n += snprintf(json + n, sizeof(json) - n, "}");
        SendResponse(client, "application/json", json, true);
    }

    void WebConfig::ServeConfigPage(WiFiClient &client)
    {
        const Settings &s = mStore->Get();

        // DE pin: -1 or 0 means "no DE pin" (auto-direction transceiver),
        // shown as an empty field
        char dePinText[8] = "";
        if (s.rtuDePin > 0)
        {
            snprintf(dePinText, sizeof(dePinText), "%d", s.rtuDePin);
        }

        char page[12288];
        int n = snprintf(page, sizeof(page),
            "<!DOCTYPE html><html lang='de'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>DeyeBridge</title><style>"
            ":root{--bg:#f3f4f1;--card:#ffffff;--text:#1f2937;--muted:#6b7280;"
            "--accent:#b45309;--accent-dark:#92400e;--ok:#15803d;--border:#e5e7eb}"
            "*{box-sizing:border-box}"
            "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
            "background:var(--bg);color:var(--text);margin:0;padding-bottom:3rem}"
            "header{background:linear-gradient(135deg,var(--accent),var(--accent-dark));"
            "color:#fff;padding:1.4rem 1rem 1.2rem;box-shadow:0 2px 8px rgba(0,0,0,.15)}"
            ".wrap{max-width:680px;margin:0 auto;padding:0 1rem}"
            "header h1{margin:0;font-size:1.35rem;font-weight:700}"
            "header p{margin:.25rem 0 0;opacity:.85;font-size:.85rem}"
            "h2{font-size:1rem;margin:0 0 .8rem;color:var(--accent-dark)}"
            ".card{background:var(--card);border:1px solid var(--border);border-radius:10px;"
            "padding:1.1rem 1.2rem;margin:1rem 0;box-shadow:0 1px 3px rgba(0,0,0,.05)}"
            "label{display:block;font-size:.8rem;font-weight:600;color:var(--muted);"
            "margin:.7rem 0 .25rem;text-transform:uppercase;letter-spacing:.03em}"
            "label:first-of-type{margin-top:0}"
            "input[type=text],input[type=password],input[type=number]{width:100%%;"
            "padding:.55rem .7rem;border:1px solid var(--border);border-radius:7px;"
            "font-size:.95rem;background:#fafafa}"
            "input:focus{outline:2px solid var(--accent);border-color:var(--accent)}"
            "input[type=file]{font-size:.85rem;margin:.4rem 0}"
            "button{background:var(--accent);color:#fff;border:none;border-radius:7px;"
            "padding:.65rem 1.4rem;font-size:.95rem;font-weight:600;cursor:pointer;margin-top:1rem}"
            "button:hover{background:var(--accent-dark)}"
            ".hint{font-size:.78rem;color:var(--muted);margin:.3rem 0 0}"
            ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:.6rem}"
            ".tile{background:#fafaf8;border:1px solid var(--border);border-radius:8px;padding:.6rem .7rem}"
            ".tile .k{font-size:.7rem;color:var(--muted);text-transform:uppercase;letter-spacing:.03em}"
            ".tile .v{font-size:1.05rem;font-weight:700;margin-top:.15rem}"
            ".status-ok{color:var(--ok)}"
            "#otastatus{font-size:.9rem;font-weight:600;margin:.5rem 0 0;min-height:1.2em}"
            "a{color:var(--accent-dark)}"
            "footer{text-align:center;color:var(--muted);font-size:.75rem;margin-top:2rem}"
            "</style></head><body>"
            "<header><div class='wrap'><h1>DeyeBridge</h1>"
            "<p>Deye Hybrid Inverter &middot; Modbus TCP + MQTT Bridge</p></div></header>"
            "<div class='wrap'>"

            "<div class='card'><h2>Live Values</h2><div class='grid' id='live'></div>"
            "<p class='hint'>Auto-refresh alle 5 s &middot; "
            "<a href='/values' target='_blank'>alle Werte als JSON</a></p></div>"

            "<div class='card'><form method='POST' action='/save'>"
            "<h2>WiFi</h2>"
            "<label>SSID</label><input type='text' name='wifi_ssid' value='%s'>"
            "<label>Password</label><input type='password' name='wifi_pass' value='%s'>"

            "<h2>MQTT</h2>"
            "<label>Broker Host</label><input type='text' name='mqtt_host' value='%s'>"
            "<label>Port</label><input type='number' name='mqtt_port' value='%u'>"
            "<label>Username</label><input type='text' name='mqtt_user' value='%s'>"
            "<label>Password</label><input type='password' name='mqtt_pass' value='%s'>"
            "<label>Client ID</label><input type='text' name='mqtt_client' value='%s'>"
            "<label>Base Topic</label><input type='text' name='mqtt_base' value='%s'>"

            "<h2>Deye RTU</h2>"
            "<label>Slave ID</label><input type='number' name='slave_id' value='%u'>"
            "<label>Baud Rate</label><input type='number' name='rtu_baud' value='%lu'>"
            "<label>TX Pin</label><input type='number' name='rtu_tx' value='%d'>"
            "<label>RX Pin</label><input type='number' name='rtu_rx' value='%d'>"
            "<label>DE Pin (optional)</label><input type='number' name='rtu_de' value='%s'>"
            "<p class='hint'>DE/RE nur bei MAX485-Modulen mit Richtungssteuerung n&ouml;tig. "
            "Leer lassen oder 0 eintragen bei Auto-Direction-Transceivern.</p>"

            "<h2>Intervalle</h2>"
            "<label>Poll Intervall (ms)</label><input type='number' name='poll_ms' value='%lu'>"
            "<label>Publish Intervall (ms)</label><input type='number' name='pub_ms' value='%lu'>"

            "<button type='submit'>Speichern &amp; Neustart</button>"
            "</form></div>"

            "<div class='card'><h2>OTA Update</h2>"
            "<p class='hint'>Firmware-Datei (.bin) hochladen &ndash; das Ger&auml;t "
            "rebootet nach Erfolg. Bei Fehlschlag bleibt die alte Firmware aktiv.</p>"
            "<input type='file' id='fwfile' accept='.bin'>"
            "<button onclick='otaUpload()'>Upload</button>"
            "<p id='otastatus'></p></div>"

            "<footer>DeyeBridge v1.0.0</footer>"
            "</div>"
            "<script>"
            "function otaUpload(){"
            "var f=document.getElementById('fwfile').files[0];"
            "if(!f){document.getElementById('otastatus').textContent='Keine Datei ausgewaehlt';return;}"
            "var s=document.getElementById('otastatus');s.textContent='Upload laeuft...';"
            "var x=new XMLHttpRequest();x.open('POST','/update');"
            "x.timeout=300000;"
            "x.onload=function(){s.textContent=x.responseText;};"
            "x.onerror=function(){s.textContent='Upload fehlgeschlagen (Verbindung verloren)';};"
            "x.send(f);}"
            "var liveLabels={pv_total_power:'PV Total (W)',battery_soc:'Batterie (%)',"
            "battery_power:'Batterie (W)',grid_power:'Netz (W)',load_power:'Verbrauch (W)',"
            "inverter_power:'WR (W)',overall_state:'Status',grid_connected:'Netz verbunden'};"
            "function refreshLive(){"
            "fetch('/values').then(function(r){return r.json();}).then(function(d){"
            "var html='';for(var k in liveLabels){"
            "var v=(typeof d[k]==='undefined')?'-':d[k];"
            "var cls=(k==='overall_state'&&v==='normal')?'status-ok':'';"
            "html+='<div class=\"tile\"><div class=\"k\">'+liveLabels[k]+'</div>"
            "<div class=\"v '+cls+'\">'+v+'</div></div>';}"
            "document.getElementById('live').innerHTML=html;})"
            ".catch(function(){});}"
            "refreshLive();setInterval(refreshLive,5000);"
            "</script></body></html>",
            s.wifiSsid, s.wifiPassword, s.mqttHost, s.mqttPort, s.mqttUser,
            s.mqttPassword, s.mqttClientId, s.mqttBaseTopic,
            s.slaveId, static_cast<unsigned long>(s.rtuBaud),
            s.rtuTxPin, s.rtuRxPin, dePinText,
            static_cast<unsigned long>(s.pollIntervalMs),
            static_cast<unsigned long>(s.publishIntervalMs));

        SendResponse(client, "text/html", page, true);
    }

    // Minimal URL decoding: %XX and '+' to space
    static void UrlDecode(char *text)
    {
        char *out = text;
        for (char *p = text; *p; ++p)
        {
            if (*p == '+')
            {
                *out++ = ' ';
            }
            else if (*p == '%' && p[1] && p[2])
            {
                char hex[3] = {p[1], p[2], '\0'};
                *out++ = static_cast<char>(strtol(hex, nullptr, 16));
                p += 2;
            }
            else
            {
                *out++ = *p;
            }
        }
        *out = '\0';
    }

    static void CopyFormField(const char *body, const char *field, char *out, size_t size)
    {
        out[0] = '\0';
        const size_t fieldLength = strlen(field);
        const char *p = body;
        while ((p = strstr(p, field)) != nullptr)
        {
            // Match the field name exactly followed by '='
            if (p[fieldLength] == '=' &&
                (p == body || p[-1] == '&'))
            {
                p += fieldLength + 1;
                const char *end = strchr(p, '&');
                const size_t length = end ? static_cast<size_t>(end - p) : strlen(p);
                const size_t copy = length < size - 1 ? length : size - 1;
                memcpy(out, p, copy);
                out[copy] = '\0';
                UrlDecode(out);
                return;
            }
            ++p;
        }
    }

    void WebConfig::HandleSave(WiFiClient &client, const char *body)
    {
        Settings &s = mStore->Get();

        CopyFormField(body, "wifi_ssid", s.wifiSsid, sizeof(s.wifiSsid));
        CopyFormField(body, "wifi_pass", s.wifiPassword, sizeof(s.wifiPassword));
        CopyFormField(body, "mqtt_host", s.mqttHost, sizeof(s.mqttHost));
        CopyFormField(body, "mqtt_user", s.mqttUser, sizeof(s.mqttUser));
        CopyFormField(body, "mqtt_pass", s.mqttPassword, sizeof(s.mqttPassword));
        CopyFormField(body, "mqtt_client", s.mqttClientId, sizeof(s.mqttClientId));
        CopyFormField(body, "mqtt_base", s.mqttBaseTopic, sizeof(s.mqttBaseTopic));

        char buffer[16];
        CopyFormField(body, "mqtt_port", buffer, sizeof(buffer));
        if (buffer[0]) { s.mqttPort = static_cast<uint16_t>(atoi(buffer)); }
        CopyFormField(body, "slave_id", buffer, sizeof(buffer));
        if (buffer[0]) { s.slaveId = static_cast<uint8_t>(atoi(buffer)); }
        CopyFormField(body, "rtu_baud", buffer, sizeof(buffer));
        if (buffer[0]) { s.rtuBaud = static_cast<uint32_t>(atol(buffer)); }
        CopyFormField(body, "rtu_tx", buffer, sizeof(buffer));
        if (buffer[0]) { s.rtuTxPin = atoi(buffer); }
        CopyFormField(body, "rtu_rx", buffer, sizeof(buffer));
        if (buffer[0]) { s.rtuRxPin = atoi(buffer); }
        CopyFormField(body, "rtu_de", buffer, sizeof(buffer));
        // Empty or 0 = auto-direction transceiver (no DE pin)
        s.rtuDePin = buffer[0] ? atoi(buffer) : -1;
        CopyFormField(body, "poll_ms", buffer, sizeof(buffer));
        if (buffer[0]) { s.pollIntervalMs = static_cast<uint32_t>(atol(buffer)); }
        CopyFormField(body, "pub_ms", buffer, sizeof(buffer));
        if (buffer[0]) { s.publishIntervalMs = static_cast<uint32_t>(atol(buffer)); }

        char response[192];
        if (mStore->Save())
        {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                     "<html><body><p>Saved. Rebooting in 3 seconds...</p></body></html>");
            client.print(response);
            client.flush();
            delay(1000);
            ESP.restart();
        }
        else
        {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                     "<html><body><p>Save failed (NVS).</p></body></html>");
            client.print(response);
        }
    }
}
