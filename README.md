# DeyeBridge

Deutsch | **English** ([README.de.md](README.de.md))

---

ESP32 firmware that reads **Deye and Sunsynk hybrid inverters** (Modbus RTU/RS485; Sunsynk uses the same register map) as a **single master** and forwards the values simultaneously to three consumers:

1. **evcc** – via a native **Modbus TCP server** (port 502, pass-through of the Deye/Sunsynk register map). This makes the evcc template `deye-hybrid-3p` work out of the box – including **battery control** (Normal/Hold/Grid charging) and **feed-in limiting (curtailment)**.
2. **Home Assistant** – via MQTT with auto-discovery (sensors, number/switch entities for control).
3. **Anything else** – via the compact JSON state message and the HTTP endpoint `/values`.

The special part: **evcc and Home Assistant can control in parallel.** All write requests (Modbus TCP FC06/FC16 and MQTT command topics) go into the same write queue, which the RTU master sends to the inverter with priority between poll cycles.

---

## Features

- Own Modbus RTU master (FC03/06/16, CRC16, DE/RE control) – no third-party Modbus libraries
- Modbus TCP server as a 1:1 register pass-through from the RAM cache (block reads like `588/38` are answered in milliseconds)
- Write whitelist: only the inverter's control registers are writable (109, 127, 128, 130, 143, 145, 146, 154–171); everything else is rejected with `ILLEGAL DATA ADDRESS`
- MQTT: individual values, JSON state, HA discovery (sensors + number/switch control entities), command topics, LWT status
- Web setup (`http://<ip>/` or fallback AP `DeyeBridge-Setup` / password `deye12345`): WiFi, MQTT, RTU pins/baud rate/slave ID, intervals; live data at `/values`
- **OTA update** via the web UI: upload a `.bin` file, the device reboots after success (watchdog is paused during the upload)
- Hardware watchdog (restarts on a hung main loop)

## Hardware

- ESP32-S3 (e.g. DevKitC-1) + MAX485 module (or a transceiver with auto-direction; leave the DE pin unconnected in that case)
- Standard pinout (changeable in the web UI):

| MAX485 | ESP32-S3 |
|--------|----------|
| DI (TXD) | GPIO 12 |
| RO (RXD) | GPIO 11 |
| DE+RE | GPIO 18 |
| A/B | Deye/Sunsynk COM port RS485 |
| GND | shared with inverter GND |

## Building & flashing the firmware

```sh
pio run -t upload          # flash
pio device monitor         # serial log, 115200 baud
pio run                    # build only
pio test -e native         # host unit tests (CRC16, queue, register map)
```

Initial flashing via USB. After that, updates are easy through the web UI: upload the firmware release (`.bin`) under **OTA Update** – the device writes the new firmware to the second app partition and reboots (on failure, the old firmware stays active).

## Connecting evcc

To the network, the device is just a normal Deye/Sunsynk inverter – evcc only queries registers (see [docs/evcc.md](docs/evcc.md), in German):

```yaml
meters:
  - name: grid
    type: template
    template: deye-hybrid-3p
    usage: grid
    modbus: tcpip          # host: IP of the ESP, port 502
    id: 1
    maxacpower: 12000
    batterytype: lv
```

## Connecting Home Assistant

Enable the MQTT integration, enter the broker connection in the DeyeBridge web UI – done. All entities appear automatically (see [docs/homeassistant.md](docs/homeassistant.md), in German).

## Architecture

```text
                            +-------------------------------------------+
 Deye/Sunsynk WR (RS485) <--+  RTU master: block polling -> RegisterCache |
                            |       ^                                    |
                            |       | write queue (prioritized)           |
                            |       v                                    |
                            |  Modbus TCP :502   MQTT (PubSubClient)     |
                            +-------+---------------------+--------------+
                                    |                     |
                                evcc (native)     Home Assistant
                                deye-hybrid-3p    (discovery + commands)
```

- **Read path**: The poller reads register blocks (matching the evcc block reads) and keeps a 1:1 register image in RAM. TCP reads are answered exclusively from the cache – network clients never block the RS485 bus.
- **Write path**: TCP and MQTT writes are checked against a whitelist, mirrored optimistically into the cache, queued, and sent by the poller as FC06 to the inverter (with read-back verification).
- **32-bit values** (energy meters) stay raw in the cache (low-word-first, like evcc `uint32s` and the usual Deye/Sunsynk documentation) – no scaling logic in the bridge.

## Known limitations

- **Not yet verified on real hardware** – before productive use, verify the values (scalings, battery current sign, register-143 units) against your own inverter firmware; register scalings vary by model and firmware version (e.g. HV models with firmware ≥ 1098: register 143 in 10 W steps, evcc option `firmware1098`).
- The MQTT switch entity "Time of Use" writes register 146 as 0x00FF/0.
  If evcc controls the battery mode at the same time, it constantly overwrites 146 –
  use HA write access to 146/127/166–171 only consciously (details in
  [docs/homeassistant.md](docs/homeassistant.md)).
- Only one RTU master per bus: putting the Deye/Sunsynk WLAN dongle on the same RS485 bus
  does not work; the hybrid models have two separate buses – dongle
  and ESP belong on separate buses.

## License

MIT – see [LICENSE](LICENSE).
