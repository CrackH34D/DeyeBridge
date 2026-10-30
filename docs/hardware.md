# Hardware-Anschluss

## Benötigte Teile

- ESP32-S3 DevKit (oder vergleichbares S3-Board)
- MAX485-Breakout (oder RS485-Transceiver mit automatischer Richtungssteuerung)
- 4–6 Jumperkabel

## Verdrahtung (Standard-Pins, im Web-UI änderbar)

| MAX485-Pin | Funktion                  | ESP32-S3 |
|------------|---------------------------|----------|
| DI         | TX des ESP (Senden)       | GPIO 12  |
| RO         | RX des ESP (Empfangen)    | GPIO 11  |
| DE + RE    | Richtungssteuerung        | GPIO 18  |
| VCC        | 3,3 V oder 5 V            | VCC      |
| GND        | Masse                     | GND      |

Transceiver mit automatischer Richtung (z. B. mit MAX13487 oder fertigen
„TTL→RS485"-Adaptern mit HW-FlowControl): DE-Pin im Web-UI leer/nicht
anschließen lassen – die Firmware toggelt den Pin nur, wenn er konfiguriert
und > 0 ist.

## Deye-Seite

Der Hybrid-Wechselrichter hat COM-Ports (RJ45) mit RS485. Am ** Batterie-/COM-Bus**
ist standardmäßig der BMS-Dongle; der zweite Port (bei 3-Phasen-Hybriden meist
„BUS 2" bzw. der Parallele-Port) trägt den 9600-8N1-Modbus-Slave.

| MAX485 | Deye RJ45 (Typ A: 1=A, 2=B) |
|--------|------------------------------|
| A      | Pin 1                        |
| B      | Pin 2                        |
| GND    | Pin 7/8 (Masse, empfohlen)   |

Pinbelegung je nach Modell prüfen (Deye-Doku des jeweiligen Hybrid-Modells).

## Wichtige Regeln

- **Nur ein RTU-Master pro Bus.** Den Deye-WLAN-Dongle und DeyeBridge an
  denselben RS485-Bus stecken funktioniert nicht. Die Hybrid-Modelle bieten
  zwei getrennte Busse – Dongle auf den einen, DeyeBridge auf den anderen.
- Bei langen Leitungen (> 5 m) 120-Ω-Abschlusswiderstand am Busende prüfen.
- Gemeinsame Masse zwischen ESP und Deye ist empfohlen.
