#pragma once

#include <cstdint>
#include <cstddef>

// Deye hybrid inverter register knowledge (3-phase LV hybrid, matches the
// register map used by evcc's "deye-hybrid-3p" template and common Deye
// Modbus documentation). Values are raw register contents; scaling lives in
// the entity tables below.

namespace DeyeRegisters
{
    // ---- Polling plan -------------------------------------------------------
    // Blocks are read back to back by the RTU master. "everyN" = read every Nth
    // poll cycle (1 = every cycle). Blocks deliberately match the block reads
    // evcc performs so a full sweep keeps the bus load minimal.

    struct RegBlock
    {
        uint16_t start;
        uint16_t count;
        uint8_t everyN;
    };

    constexpr RegBlock kPollBlocks[] = {
        {104, 68, 3},   // settings/write registers 104..171 (battery limits, TOU)
        {500, 2, 3},    // overall state
        {514, 25, 3},   // energy counters 514..538
        {540, 2, 3},    // temperatures
        {586, 53, 1},   // battery + grid + inverter 586..638
        {650, 4, 1},    // load powers 650..653
        {667, 9, 1},    // gen + pv powers 667..675
        {676, 8, 3},    // pv voltages/currents 676..683
    };

    constexpr size_t kPollBlockCount = sizeof(kPollBlocks) / sizeof(kPollBlocks[0]);

    // ---- Writable registers (whitelist) --------------------------------------
    // Only these addresses may be written via Modbus TCP (evcc) or MQTT (Home
    // Assistant). Everything else is answered with ILLEGAL DATA ADDRESS.

    constexpr uint16_t kWritableRegisters[] = {
        109,   // battery max discharge current, A
        127,   // battery grid charging start SOC, %
        128,   // battery grid charging current, A
        130,   // battery grid charging enable, 0/1
        143,   // max sell power, W (LV models: 1 W units)
        145,   // solar sell enable, bitmask (bit 0)
        146,   // time-of-use enable, 0x00FF = all days enabled
        154, 155, 156, 157, 158, 159,   // time slot power 1..6, W
        160, 161, 162, 163, 164, 165,   // time slot voltage/SOC 1..6
        166, 167, 168, 169, 170, 171,   // time slot SOC 1..6, %
    };

    constexpr size_t kWritableRegisterCount =
        sizeof(kWritableRegisters) / sizeof(kWritableRegisters[0]);

    bool IsWritable(uint16_t address);

    // ---- MQTT / Home Assistant entities --------------------------------------

    // A numeric sensor published on {base}/sensor/{key}
    struct SensorEntity
    {
        const char *key;
        const char *name;
        uint16_t address;
        float scale;          // raw * scale + offset = engineering value
        float offset;
        const char *unit;     // nullptr = none
        const char *deviceClass;    // nullptr = none
        const char *stateClass;     // nullptr = none
        bool isSigned;
        bool isDword;         // 32-bit, low word first (matches evcc "uint32s")
    };

    // A writable value published on {base}/sensor/{key} and controlled via
    // {base}/set/{key} (number) or ON/OFF payload (switch)
    struct ControlEntity
    {
        const char *key;
        const char *name;
        uint16_t address;
        float scale;          // raw = value / scale
        const char *unit;
        float minValue;
        float maxValue;
        float step;
        bool isSwitch;
        uint16_t onValue;     // raw value written for switch ON
        uint16_t offValue;    // raw value written for switch OFF
        const char *icon;
    };

    constexpr SensorEntity kSensors[] = {
        // PV strings
        {"pv1_power",     "PV1 Power",          672, 1,    0,     "W",  "power",         "measurement",      false, false},
        {"pv2_power",     "PV2 Power",          673, 1,    0,     "W",  "power",         "measurement",      false, false},
        {"pv3_power",     "PV3 Power",          674, 1,    0,     "W",  "power",         "measurement",      false, false},
        {"pv4_power",     "PV4 Power",          675, 1,    0,     "W",  "power",         "measurement",      false, false},
        {"pv1_voltage",   "PV1 Voltage",        676, 0.1,  0,     "V",  "voltage",       "measurement",      false, false},
        {"pv2_voltage",   "PV2 Voltage",        678, 0.1,  0,     "V",  "voltage",       "measurement",      false, false},
        {"pv3_voltage",   "PV3 Voltage",        680, 0.1,  0,     "V",  "voltage",       "measurement",      false, false},
        {"pv4_voltage",   "PV4 Voltage",        682, 0.1,  0,     "V",  "voltage",       "measurement",      false, false},
        {"pv1_current",   "PV1 Current",        677, 0.1,  0,     "A",  "current",       "measurement",      false, false},
        {"pv2_current",   "PV2 Current",        679, 0.1,  0,     "A",  "current",       "measurement",      false, false},
        {"pv3_current",   "PV3 Current",        681, 0.1,  0,     "A",  "current",       "measurement",      false, false},
        {"pv4_current",   "PV4 Current",        683, 0.1,  0,     "A",  "current",       "measurement",      false, false},

        // Battery
        {"battery_soc",         "Battery SOC",           588, 1,    0,        "%",   "battery",     "measurement",       false, false},
        {"battery_voltage",     "Battery Voltage",       587, 0.1,  0,        "V",   "voltage",     "measurement",       false, false},
        {"battery_power",       "Battery Power",         590, 1,    0,        "W",   "power",       "measurement",       true,  false},
        {"battery_current",     "Battery Current",       591, 0.1,  0,        "A",   "current",     "measurement",       true,  false},
        {"battery_temperature", "Battery Temperature",   586, 0.1,  -1000.0f, "\xc2\xb0""C", "temperature", "measurement", false, false},

        // Grid
        {"grid_power",        "Grid Power",        625, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_l1_power",     "Grid L1 Power",     622, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_l2_power",     "Grid L2 Power",     623, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_l3_power",     "Grid L3 Power",     624, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_voltage_l1",   "Grid L1 Voltage",   598, 0.1,  0, "V",  "voltage", "measurement", false, false},
        {"grid_voltage_l2",   "Grid L2 Voltage",   599, 0.1,  0, "V",  "voltage", "measurement", false, false},
        {"grid_voltage_l3",   "Grid L3 Voltage",   600, 0.1,  0, "V",  "voltage", "measurement", false, false},
        {"grid_current_l1",   "Grid L1 Current",   610, 0.1,  0, "A",  "current", "measurement", true,  false},
        {"grid_current_l2",   "Grid L2 Current",   611, 0.1,  0, "A",  "current", "measurement", true,  false},
        {"grid_current_l3",   "Grid L3 Current",   612, 0.1,  0, "A",  "current", "measurement", true,  false},
        {"grid_frequency",    "Grid Frequency",    609, 0.01, 0, "Hz", nullptr,   "measurement", false, false},
        {"ct_power",          "Grid CT Power",     619, 1,    0, "W",  "power",   "measurement", true,  false},

        // Inverter / load / gen
        {"inverter_power",     "Inverter Power",     636, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_l1_power",  "Inverter L1 Power",  633, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_l2_power",  "Inverter L2 Power",  634, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_l3_power",  "Inverter L3 Power",  635, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_frequency", "Inverter Frequency", 638, 0.01, 0,     "Hz", nullptr,   "measurement", false, false},
        {"load_power",         "Load Power",         653, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"load_l1_power",      "Load L1 Power",      650, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"load_l2_power",      "Load L2 Power",      651, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"load_l3_power",      "Load L3 Power",      652, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"gen_power",          "Gen Port Power",     667, 1,    0,     "W",  "power",   "measurement", true,  false},

        // Energy counters (32-bit, low word first)
        {"day_battery_charge",       "Day Battery Charge",    514, 0.1, 0, "kWh", "energy", "total_increasing", false, false},
        {"day_battery_discharge",    "Day Battery Discharge", 515, 0.1, 0, "kWh", "energy", "total_increasing", false, false},
        {"total_battery_charge",     "Total Battery Charge",  516, 0.1, 0, "kWh", "energy", "total_increasing", false, true},
        {"total_battery_discharge",  "Total Battery Discharge", 518, 0.1, 0, "kWh", "energy", "total_increasing", false, true},
        {"day_grid_import",          "Day Grid Import",       520, 0.1, 0, "kWh", "energy", "total_increasing", false, false},
        {"day_grid_export",          "Day Grid Export",       521, 0.1, 0, "kWh", "energy", "total_increasing", false, false},
        {"total_grid_import",        "Total Grid Import",     522, 0.1, 0, "kWh", "energy", "total_increasing", false, true},
        {"total_grid_export",        "Total Grid Export",     524, 0.1, 0, "kWh", "energy", "total_increasing", false, true},
        {"day_load_energy",          "Day Load Energy",       526, 0.1, 0, "kWh", "energy", "total_increasing", false, false},
        {"total_load_energy",        "Total Load Energy",     527, 0.1, 0, "kWh", "energy", "total_increasing", false, true},
        {"day_pv_energy",            "Day PV Energy",         529, 0.1, 0, "kWh", "energy", "total_increasing", false, false},
        {"total_pv_energy",          "Total PV Energy",       534, 0.1, 0, "kWh", "energy", "total_increasing", false, true},

        // Temperatures
        {"dc_transformer_temperature", "DC Transformer Temperature", 540, 0.1, -1000.0f, "\xc2\xb0""C", "temperature", "measurement", true, false},
        {"radiator_temperature",       "Radiator Temperature",       541, 0.1, -1000.0f, "\xc2\xb0""C", "temperature", "measurement", true, false},
    };

    constexpr size_t kSensorCount = sizeof(kSensors) / sizeof(kSensors[0]);

    constexpr ControlEntity kControls[] = {
        {"max_sell_power",               "Max Sell Power",               143, 1,   "W",  0,    12000, 100, false, 1,     0,     "mdi:solar-power"},
        {"battery_max_discharge_current", "Battery Max Discharge Current", 109, 1,  "A",  0,    210,   5,   false, 0,     0,     "mdi:battery-arrow-down"},
        {"grid_charge_current",          "Grid Charge Current",          128, 1,   "A",  0,    210,   5,   false, 0,     0,     "mdi:battery-charging"},
        {"grid_charge_start_soc",        "Grid Charge Start SOC",        127, 1,   "%",  0,    100,   1,   false, 0,     0,     "mdi:battery-charging-medium"},
        {"solar_sell",                   "Solar Sell",                   145, 1,   nullptr, 0, 1,     1,   true,  1,     0,     "mdi:transmission-tower-export"},
        {"time_of_use",                  "Time of Use",                  146, 1,   nullptr, 0, 1,     1,   true,  0x00FF, 0,    "mdi:clock-outline"},
    };

    constexpr size_t kControlCount = sizeof(kControls) / sizeof(kControls[0]);

    // Overall state register (500) mapped to text
    const char *OverallStateName(uint16_t raw);
}
