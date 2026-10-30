# DeyeBridge mit Home Assistant verbinden

DeyeBridge publishes Werte und Entity-Definitionen per MQTT. Sobald die
MQTT-Integration in Home Assistant aktiv ist und der Broker im Web-UI
eingetragen wurde, erscheinen alle Entities automatisch (MQTT Discovery).

## Topics

| Zweck | Topic |
|-------|-------|
| Sensorwerte | `<base>/sensor/<key>` (z. B. `deye/sensor/battery_soc`) |
| JSON-Gesamtstatus | `<base>/state` |
| Verfügbarkeit (LWT) | `<base>/status` (`online`/`offline`, retained) |
| Steuerung | `<base>/set/<key>` (z. B. `deye/set/max_sell_power`) |
| HA-Discovery | `homeassistant/<typ>/deye-bridge/<key>/config` (retained) |

## Sensor-Entities (Auszug)

- `pv1_power` … `pv4_power`, `pv_total_power`
- `battery_soc`, `battery_voltage`, `battery_power`, `battery_current`, `battery_temperature`
- `grid_power`, `grid_l1_power` … `grid_l3_power`, `grid_voltage_l1` … `grid_voltage_l3`,
  `grid_current_l1` … `grid_current_l3`, `grid_frequency`, `ct_power`
- `inverter_power`, `inverter_l1_power` … `inverter_l3_power`, `inverter_frequency`
- `load_power`, `load_l1_power` … `load_l3_power`, `gen_power`
- Energiezähler: `day_*` und `total_*` (kWh, `total_increasing`)
- `dc_transformer_temperature`, `radiator_temperature`, `overall_state` (Text)

## Steuer-Entities

| Entity | Register | Typ | Bereich |
|--------|----------|-----|---------|
| Max Sell Power | 143 | number | 0–12000 W, Schritt 100 |
| Battery Max Discharge Current | 109 | number | 0–210 A, Schritt 5 |
| Grid Charge Current | 128 | number | 0–210 A, Schritt 5 |
| Grid Charge Start SOC | 127 | number | 0–100 % |
| Solar Sell | 145 | switch | ON/OFF (Bit 0) |
| Time of Use | 146 | switch | ON = 0x00FF (alle Tage), OFF = 0 |

Beispiel Steuerung per MQTT:

```text
deye/set/max_sell_power  ->  "5000"
deye/set/solar_sell      ->  "ON"
```

## Gleichzeitige Steuerung mit evcc

Beide Systeme schreiben in dieselbe Write-Queue – **letzter Befehl gewinnt**.
Damit sich evcc und HA nicht gegenseitig stören:

- evcc überschreibt im Batteriemodus laufend die Register 127, 146 und 166–171.
  HA-Steuer-Entities für diese Register sinnvoll **nur lesend** nutzen oder
  nur dann schreiben, wenn evcc den Batteriemodus gerade nicht aktiv steuert.
- Konfliktfrei parallel: Register 143 (Max Sell Power), 145 (Solar Sell) und
  154–165 (Time-Slot-Parameter) werden von evcc nicht angefasst.

## batteriespezifische Automatisierung

Für Batteriesteuerung komplett über HA (ohne evcc-Batterie-Treiber) genügt es,
die Number-/Switch-Entities in Automatisierungen zu setzen – z. B. Netzladung:

1. `deye/set/grid_charge_current` = 60
2. `deye/set/grid_charge_start_soc` = 80
3. `deye/set/time_of_use` = OFF  (Register 130 im Deye steuert das Grid-Charging
   selbst; Register 146/TOU-Steuerung je nach gewünschtem Verhalten)
