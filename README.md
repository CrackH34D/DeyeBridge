# DeyeBridge

ESP32-Firmware, die einen **Deye/Sunsynk Hybrid-Wechselrichter** (Modbus RTU/RS485) als **einzelner Master** ausliest und die Werte gleichzeitig an drei Konsumenten weiterreicht:

1. **evcc** – über einen nativen **Modbus-TCP-Server** (Port 502, Pass-Through des Deye-Register-Maps). Damit funktioniert das evcc-Template `deye-hybrid-3p` direkt – inklusive **Batteriesteuerung** (Normal/Hold/Netzladung) und **Einspeisebegrenzung (Curtailment)**.
2. **Home Assistant** – über MQTT mit Auto-Discovery (Sensoren, Number-/Switch-Entities für Steuerung).
3. **Beliebig** – über die kompakte JSON-State-Message und den HTTP-Endpunkt `/values`.

Das Besondere: **evcc und Home Assistant können parallel steuern.** Alle Schreibzugriffe (Modbus TCP FC06/FC16 und MQTT-Command-Topics) landen in derselben Write-Queue, die der RTU-Master priorisiert zwischen den Poll-Zyklen an den Wechselrichter schickt.

---

## Features

- Eigener Modbus-RTU-Master (FC03/06/16, CRC16, DE/RE-Steuerung) – keine fremden Modbus-Libraries
- Modbus-TCP-Server als 1:1-Register-Pass-Through aus dem RAM-Cache (Block-Reads wie `588/38` werden in Millisekunden beantwortet)
- Schreib-Whitelist: nur die Steuerregister des Wechselrichters sind beschreibbar (109, 127, 128, 130, 143, 145, 146, 154–171), alles andere wird mit `ILLEGAL DATA ADDRESS` abgelehnt
- MQTT: Einzelwerte, JSON-State, HA-Discovery (Sensoren + Number/Switch-Steuer-Entities), Command-Topics, LWT-Status
- Web-Setup (`http://<ip>/` oder Fallback-AP `DeyeBridge-Setup` / Passwort `deye12345`): WiFi, MQTT, RTU-Pins/Baudrate/Slave-ID, Intervalle; Live-Daten unter `/values`
- **OTA-Update** über das Web-UI: `.bin`-Datei hochladen, Gerät rebootet nach Erfolg (Watchdog wird während des Uploads pausiert)
- Hardware-Watchdog (restartet bei hängendem Haupt-Loop)

## Hardware

- ESP32-S3 (z. B. DevKitC-1) + MAX485-Modul (oder Transceiver mit Auto-Direction, dann DE-Pin leer lassen)
- Standard-Pinbelegung (im Web-UI änderbar):

| MAX485 | ESP32-S3 |
|--------|----------|
| DI (TXD) | GPIO 12 |
| RO (RXD) | GPIO 11 |
| DE+RE | GPIO 18 |
| A/B | Deye COM-Port RS485 |
| GND | gemeinsam mit Deye-GND |

## Firmware bauen & flashen

```sh
pio run -t upload          # flashen
pio device monitor         # Serial-Log, 115200 baud
pio run                    # nur bauen
pio test -e native         # Host-Unit-Tests (CRC16, Queue, Registerplan)
```

Erstflash über USB. Danach Updates bequem über das Web-UI: Firmware-Release
(`.bin`) unter **OTA Update** hochladen – das Gerät schreibt die neue Firmware
in die zweite App-Partition und rebootet (bei Fehlschlag bleibt die alte
Firmware aktiv).

## evcc anbinden

Das Gerät ist ein ganz normaler Deye-Wechselrichter im Netz – evcc fragt nichts weiter ab als Register (siehe [docs/evcc.md](docs/evcc.md)):

```yaml
meters:
  - name: grid
    type: template
    template: deye-hybrid-3p
    usage: grid
    modbus: tcpip          # Host: IP des ESP, Port 502
    id: 1
    maxacpower: 12000
    batterytype: lv
```

## Home Assistant anbinden

MQTT-Integration aktivieren, Broker-Verbindung im DeyeBridge-Web-UI eintragen – fertig. Alle Entities erscheinen automatisch (siehe [docs/homeassistant.md](docs/homeassistant.md)).

## Architektur

```text
                    +-------------------------------------------+
 Deye WR (RS485) <--+  RTU-Master: Block-Polling -> RegisterCache |
                    |       ^                                    |
                    |       | Write-Queue (priorisiert)          |
                    |       v                                    |
                    |  Modbus TCP :502   MQTT (PubSubClient)     |
                    +-------+---------------------+--------------+
                            |                     |
                        evcc (nativ)      Home Assistant
                        deye-hybrid-3p    (Discovery + Commands)
```

- **Lese-Pfad**: Der Poller liest Registerblöcke (entsprechen den evcc-Block-Reads) und hält ein 1:1-Register-Image im RAM. TCP-Reads antworten ausschließlich aus dem Cache – Netz-Clients blockieren nie den RS485-Bus.
- **Schreib-Pfad**: TCP- und MQTT-Writes werden gegen eine Whitelist geprüft, optimistic in den Cache gespiegelt, in die Queue gestellt und vom Poller als FC06 an den Wechselrichter gesendet (mit Read-back-Verifikation).
- **32-Bit-Werte** (Energiezähler) bleiben roh im Cache (Low-Word-First, wie evcc `uint32s` und die üblichen Deye-Dokumentationen) – keine Skalierungslogik in der Brücke.

## Bekannte Einschränkungen

- **Noch nicht an realer Hardware verifiziert** – vor dem Produktivbetrieb die
  Werte (Skalierungen, Battery-Strom-Vorzeichen, Register-143-Einheiten)
  gegen die eigene Inverter-Firmware prüfen; Register-Skalierungen variieren
  je nach Modell und Firmware-Stand (z. B. HV-Modelle mit Firmware ≥ 1098:
  Register 143 in 10-W-Schritten, evcc-Option `firmware1098`).
- Das MQTT-Switch-Entity „Time of Use" schreibt Register 146 als 0x00FF/0.
  Wenn evcc gleichzeitig Batteriemodus steuert, überschreibt es 146 laufend –
  HA-Schreibzugriff auf 146/127/166–171 nur mit Bewusstsein (Details in
  [docs/homeassistant.md](docs/homeassistant.md)).
- Nur ein RTU-Master pro Bus: den Deye-WLAN-Dongle an denselben RS485-Bus
  stecken geht nicht; die Hybrid-Modelle haben zwei getrennte Busse – Dongle
  und ESP gehören auf getrennte Busse.

## Lizenz

MIT – siehe [LICENSE](LICENSE).
