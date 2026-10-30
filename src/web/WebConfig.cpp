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

        char page[16384];
        int n = snprintf(page, sizeof(page),
            "<!DOCTYPE html><html lang='de'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<link rel='icon' href=\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'><rect width='64' height='64' rx='14' fill='%23b45309'/><path d='M36 8L18 36h12l-4 20 20-28H32l4-20z' fill='%23ffffff'/></svg>\">"
            "<title>DeyeBridge</title><style>"
            ":root{--bg:#f6f7f4;--card:#ffffff;--text:#111827;--muted:#6b7280;"
            "--accent:#b45309;--accent2:#d97706;--accent-soft:#fdf0dd;--ok:#15803d;"
            "--border:#e5e7eb;--shadow:0 1px 3px rgba(16,24,32,.06),0 10px 30px rgba(16,24,32,.07)}"
            "@media(prefers-color-scheme:dark){:root{--bg:#0e1114;--card:#171c22;--text:#e5e7eb;"
            "--muted:#9aa3af;--accent:#f59e0b;--accent2:#fbbf24;--accent-soft:#2b2110;"
            "--ok:#4ade80;--border:#272d35;--shadow:0 1px 3px rgba(0,0,0,.45)}}"
            "*{box-sizing:border-box}html{scroll-behavior:smooth}"
            "body{margin:0;font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;"
            "background:var(--bg);color:var(--text)}"
            ".wrap{max-width:720px;margin:0 auto;padding:0 1.1rem}"
            "header{position:sticky;top:0;z-index:10;color:#fff;"
            "background:linear-gradient(135deg,#b45309,#7c2d12);box-shadow:var(--shadow)}"
            "header .wrap{display:flex;align-items:center;justify-content:space-between;padding:.9rem 1.1rem}"
            "h1{font-size:1.15rem;margin:0;font-weight:800;letter-spacing:-.01em}"
            ".sub{font-size:.72rem;opacity:.8;margin:.15rem 0 0}"
            ".badge{background:rgba(255,255,255,.16);padding:.3rem .7rem;border-radius:999px;"
            "font-size:.72rem;font-weight:700;letter-spacing:.04em}"
            "nav{position:sticky;top:64px;z-index:9;background:var(--bg);border-bottom:1px solid var(--border)}"
            "nav .wrap{display:flex;gap:.35rem;padding:.45rem 1.1rem}"
            "nav a{color:var(--muted);text-decoration:none;font-size:.82rem;font-weight:700;"
            "padding:.35rem .85rem;border-radius:999px;white-space:nowrap;transition:all .15s}"
            "nav a:hover{color:var(--text);background:var(--accent-soft)}"
            ".card{background:var(--card);border:1px solid var(--border);border-radius:16px;"
            "padding:1.15rem 1.25rem;margin:1rem 0;box-shadow:var(--shadow)}"
            "h2{display:flex;align-items:center;gap:.5rem;font-size:.95rem;margin:0 0 1rem}"
            "h2 svg{color:var(--accent);flex:none}"
            ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:.65rem}"
            ".tile{border:1px solid var(--border);border-radius:12px;padding:.7rem .85rem;background:var(--bg)}"
            ".tile .k{font-size:.66rem;color:var(--muted);text-transform:uppercase;"
            "letter-spacing:.06em;font-weight:700}"
            ".tile .v{font-size:1.15rem;font-weight:800;margin-top:.25rem;"
            "font-variant-numeric:tabular-nums}"
            ".tile .v.ok{color:var(--ok)}"
            ".row2{display:grid;grid-template-columns:1fr 1fr;gap:0 .9rem}"
            ".field{margin-bottom:.9rem}"
            "label{display:block;font-size:.7rem;font-weight:700;color:var(--muted);"
            "margin-bottom:.3rem;text-transform:uppercase;letter-spacing:.06em}"
            "input{width:100%%;padding:.6rem .75rem;border:1px solid var(--border);"
            "border-radius:10px;font-size:.95rem;background:var(--bg);color:var(--text);"
            "transition:border-color .15s,box-shadow .15s}"
            "input:focus{outline:none;border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-soft)}"
            "button{background:var(--accent);color:#fff;border:none;border-radius:10px;"
            "padding:.7rem 1.5rem;font-size:.92rem;font-weight:700;cursor:pointer;"
            "transition:filter .15s,transform .05s}"
            "button:hover{filter:brightness(1.1)}"
            "button:active{transform:scale(.98)}"
            ".hint{font-size:.77rem;color:var(--muted);margin:.4rem 0 0;line-height:1.45}"
            "#otastatus{font-weight:700;min-height:1.2em;margin:.6rem 0 0}"
            "footer{text-align:center;color:var(--muted);font-size:.75rem;padding:2rem 0}"
            "@media(max-width:520px){.row2{grid-template-columns:1fr}}"
            "</style></head><body>"
            "<header><div class='wrap'><div><h1>DeyeBridge</h1>"
            "<p class='sub'>Deye Hybrid Inverter &middot; Modbus TCP + MQTT Bridge</p></div>"
            "<span class='badge' id='conn'>&ndash;</span></div></header>"
            "<nav><div class='wrap'><a href='#live'>Live</a>"
            "<a href='#setup'>Setup</a><a href='#ota'>OTA</a></div></nav>"
            "<div class='wrap'>"

            "<div class='card' id='live'><h2>"
            "<svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M13 2L3 14h9l-1 8 10-12h-9l1-8z'/></svg>"
            "Live Values</h2><div class='grid' id='livegrid'></div>"
            "<p class='hint'>Auto-refresh alle 5 s &middot; "
            "<a href='/values' target='_blank'>alle Werte als JSON</a></p></div>"

            "<div class='card' id='setup'><form method='POST' action='/save'>"
            "<h2><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M5 12.55a11 11 0 0 1 14.08 0'/><path d='M1.42 9a16 16 0 0 1 21.16 0'/><path d='M8.53 16.11a6 6 0 0 1 6.95 0'/><line x1='12' y1='20' x2='12.01' y2='20'/></svg>"
            "WiFi</h2>"
            "<div class='row2'><div class='field'><label>SSID</label>"
            "<input type='text' name='wifi_ssid' value='%s'></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='wifi_pass' value='%s'></div></div>"

            "<h2><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><circle cx='12' cy='12' r='2'/><path d='M16.24 7.76a6 6 0 0 1 0 8.49m-8.48-.01a6 6 0 0 1 0-8.49m11.31-2.82a10 10 0 0 1 0 14.14m-14.14 0a10 10 0 0 1 0-14.14'/></svg>"
            "MQTT</h2>"
            "<div class='row2'><div class='field'><label>Broker Host</label>"
            "<input type='text' name='mqtt_host' value='%s'></div>"
            "<div class='field'><label>Port</label>"
            "<input type='number' name='mqtt_port' value='%u'></div></div>"
            "<div class='row2'><div class='field'><label>Username</label>"
            "<input type='text' name='mqtt_user' value='%s'></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='mqtt_pass' value='%s'></div></div>"
            "<div class='row2'><div class='field'><label>Client ID</label>"
            "<input type='text' name='mqtt_client' value='%s'></div>"
            "<div class='field'><label>Base Topic</label>"
            "<input type='text' name='mqtt_base' value='%s'></div></div>"

            "<h2><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><rect x='4' y='4' width='16' height='16' rx='2'/><rect x='9' y='9' width='6' height='6'/><line x1='9' y1='1' x2='9' y2='4'/><line x1='15' y1='1' x2='15' y2='4'/><line x1='9' y1='20' x2='9' y2='23'/><line x1='15' y1='20' x2='15' y2='23'/><line x1='20' y1='9' x2='23' y2='9'/><line x1='20' y1='15' x2='23' y2='15'/><line x1='1' y1='9' x2='4' y2='9'/><line x1='1' y1='15' x2='4' y2='15'/></svg>"
            "Deye RTU</h2>"
            "<div class='row2'><div class='field'><label>Slave ID</label>"
            "<input type='number' name='slave_id' value='%u'></div>"
            "<div class='field'><label>Baud Rate</label>"
            "<input type='number' name='rtu_baud' value='%lu'></div></div>"
            "<div class='row2'><div class='field'><label>TX Pin</label>"
            "<input type='number' name='rtu_tx' value='%d'></div>"
            "<div class='field'><label>RX Pin</label>"
            "<input type='number' name='rtu_rx' value='%d'></div></div>"
            "<div class='field'><label>DE Pin (optional)</label>"
            "<input type='number' name='rtu_de' value='%s'>"
            "<p class='hint'>DE/RE nur bei MAX485-Modulen mit Richtungssteuerung n&ouml;tig. "
            "Leer lassen oder 0 eintragen bei Auto-Direction-Transceivern.</p></div>"

            "<h2><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><circle cx='12' cy='12' r='10'/><polyline points='12 6 12 12 16 14'/></svg>"
            "Intervalle</h2>"
            "<div class='row2'><div class='field'><label>Poll Intervall (ms)</label>"
            "<input type='number' name='poll_ms' value='%lu'></div>"
            "<div class='field'><label>Publish Intervall (ms)</label>"
            "<input type='number' name='pub_ms' value='%lu'></div></div>"

            "<button type='submit'>Speichern &amp; Neustart</button>"
            "</form></div>"

            "<div class='card' id='ota'><h2>"
            "<svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4'/><polyline points='17 8 12 3 7 8'/><line x1='12' y1='3' x2='12' y2='15'/></svg>"
            "OTA Update</h2>"
            "<p class='hint'>Firmware-Datei (.bin) hochladen &ndash; das Ger&auml;t "
            "rebootet nach Erfolg. Bei Fehlschlag bleibt die alte Firmware aktiv.</p>"
            "<input type='file' id='fwfile' accept='.bin'>"
            "<button onclick='otaUpload()'>Upload</button>"
            "<p id='otastatus'></p></div>"

            "<footer>DeyeBridge v1.0.0</footer></div>"
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
            "document.getElementById('conn').textContent='live';"
            "var html='';for(var k in liveLabels){"
            "var v=(typeof d[k]==='undefined')?'-':d[k];"
            "var cls=(k==='overall_state'&&v==='normal')?' ok':'';"
            "html+='<div class=\"tile\"><div class=\"k\">'+liveLabels[k]+'</div>"
            "<div class=\"v'+cls+'\">'+v+'</div></div>';}"
            "document.getElementById('livegrid').innerHTML=html;})"
            ".catch(function(){document.getElementById('conn').textContent='offline';});}"
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
