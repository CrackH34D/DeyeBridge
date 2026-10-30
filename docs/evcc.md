# DeyeBridge mit evcc verbinden

DeyeBridge stellt den Wechselrichter als normalen Modbus-TCP-Teilnehmer bereit,
sodass evcc das **native Template `deye-hybrid-3p`** verwenden kann. Es wird
nichts emuliert und nichts übersetzt – die TCP-Register sind die Deye-Register.

## Voraussetzungen

- DeyeBridge läuft und ist per Modbus TCP erreichbar (Port 502)
- RTU-Verbindung zum Wechselrichter steht (Web-UI `/values` zeigt Live-Werte)

## Konfiguration

In evcc (YAML oder UI „Wechselrichter hinzufügen"):

```yaml
meters:
  - name: grid
    type: template
    template: deye-hybrid-3p
    usage: grid
    modbus: tcpip
    id: 1                  # Slave-ID wie im Web-UI konfiguriert (Standard 1)
    host: 192.168.1.50     # IP des DeyeBridge-ESP
    port: 502
    maxacpower: 12000      # Nennleistung des Wechselrichters
    batterytype: lv        # lv bis 60 V Nennspannung, sonst hv

  - name: pv
    type: template
    template: deye-hybrid-3p
    usage: pv
    modbus: tcpip
    id: 1
    host: 192.168.1.50
    port: 502
    maxacpower: 12000
    batterytype: lv

  - name: battery
    type: template
    template: deye-hybrid-3p
    usage: battery
    modbus: tcpip
    id: 1
    host: 192.168.1.50
    port: 502
    maxacpower: 12000
    batterytype: lv
    capacity: 10           # Batteriekapazität in Ah
    minsoc: 10
    maxsoc: 95
    maxdischargecurrent: 100   # Register 109 im Normalbetrieb
    gridchargecurrent: 60      # Register 128 im Netzlademodus
```

Im evcc-UI statt `modbus: tcpip` einfach „Netzwerk (Modbus TCP)" wählen und
Host/Port eintragen.

## Was dadurch möglich wird

Das Template meldet `capabilities: ["battery-control", "curtail"]`:

- **Batteriemodi**: evcc schreibt beim Umschalten (Normal/Hold/Netzladung) die
  Register 127, 128, 130, 146, 166–171 und 109. DeyeBridge validiert die
  Adressen, spiegelt sie in den Cache und setzt sie priorisiert per RTU ab.
- **Einspeisebegrenzung (Curtailment)**: evcc limitiert die Einspeisung über
  Register 143 (Max Sell Power), z. B. beim Curtailment-Modus.

## Hinweise

- Poll-Rate: evcc fragt standardmäßig ~1-s-zyklisch. DeyeBridge antwortet aus
  dem RAM-Cache, der RS485-Bus wird dabei nicht belastet – die Poll-Rate muss
  nicht reduziert werden.
- `batterytype: hv` und Firmware ≥ 1098: zusätzlich die evcc-Option
  `firmware1098` aktivieren (veränderte Skalierung von 143 und Energiezählern).
- Die PV-Gesamtleistung addiert Register 672–675 (entspricht den Sensoren
  `pv1_power` … `pv4_power` im MQTT).
