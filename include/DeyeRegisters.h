#pragma once

#include <cstdint>
#include <cstddef>

// Deye hybrid inverter register knowledge (3-phase LV hybrid, register map of
// the user's proven ESPHome config plus the evcc "deye-hybrid-3p" template).
// Values are raw register contents; scaling lives in the entity tables below.

namespace DeyeRegisters
{
    // ---- Polling plan -------------------------------------------------------
    // Blocks are read back to back by the RTU master. "everyN" = read every Nth
    // poll cycle (1 = every cycle).

    struct RegBlock
    {
        uint16_t start;
        uint16_t count;
        uint8_t everyN;
    };

    constexpr RegBlock kPollBlocks[] = {
        {80, 1, 3},     // inverter on/off state
        {104, 68, 3},   // settings/write registers 104..171 (limits, TOU, slots)
        {172, 6, 3},    // grid charge time enables 172..177
        {500, 2, 3},    // overall state
        {514, 25, 3},   // energy counters 514..538
        {540, 2, 3},    // temperatures
        {552, 1, 3},    // grid connected status
        {586, 53, 1},   // battery + grid + inverter 586..638
        {650, 4, 1},    // load powers 650..653
        {664, 12, 1},   // gen L1..L3 (664..666) + pv powers (667..675)
        {676, 8, 3},    // pv voltages/currents 676..683
    };

    constexpr size_t kPollBlockCount = sizeof(kPollBlocks) / sizeof(kPollBlocks[0]);

    // ---- Writable registers (whitelist) --------------------------------------
    // Only these addresses may be written via Modbus TCP (evcc) or MQTT (Home
    // Assistant). Everything else is answered with ILLEGAL DATA ADDRESS.

    constexpr uint16_t kWritableRegisters[] = {
        80,    // inverter on/off, bitmask (bit 0)
        104,   // zero export, signed
        108,   // battery max charge current, A
        109,   // battery max discharge current, A
        127,   // battery grid charging start SOC, %
        128,   // battery grid charging current, A
        130,   // battery grid charging enable, 0/1
        141,   // energy pattern, 0/1
        142,   // work mode, 0..2
        143,   // max sell power, W (LV models: 1 W units)
        145,   // solar sell enable, bitmask (bit 0)
        146,   // time-of-use enable, 0x00FF = all days enabled
        154, 155, 156, 157, 158, 159,   // time slot power 1..6, W
        160, 161, 162, 163, 164, 165,   // time slot voltage 1..6 (V x 10)
        166, 167, 168, 169, 170, 171,   // time slot SOC 1..6, %
        172, 173, 174, 175, 176, 177,   // grid charge time enable 1..6, bitmask
    };

    constexpr size_t kWritableRegisterCount =
        sizeof(kWritableRegisters) / sizeof(kWritableRegisters[0]);

    bool IsWritable(uint16_t address);

    // ---- Entity types ---------------------------------------------------------

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

    // An on/off state published on {base}/sensor/{key} as ON/OFF
    struct BinaryEntity
    {
        const char *key;
        const char *name;
        uint16_t address;
    };

    // A text sensor computed from a raw register
    struct TextEntity
    {
        const char *key;
        const char *name;
        uint16_t address;
    };

    struct SelectOption
    {
        const char *label;
        uint16_t value;
    };

    // A writable value published on {base}/sensor/{key} and controlled via
    // {base}/set/{key}. isSwitch: ON/OFF payloads; isSelect: label payloads;
    // otherwise a HA number. clearMask enables masked read-modify-write for
    // registers where only some bits may be touched (0xFFFF = full write).
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
        uint16_t onValue;
        uint16_t offValue;
        bool isSelect;
        const SelectOption *options;
        size_t optionCount;
        bool isSigned;
        uint16_t clearMask;
        const char *icon;
    };

    // ---- Sensor table ----------------------------------------------------------

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
        {"battery_soc",         "Battery SOC",           588, 1,    0,       "%",   "battery",     "measurement",       false, false},
        {"battery_voltage",     "Battery Voltage",       587, 0.01, 0,       "V",   "voltage",     "measurement",       false, false},
        {"battery_power",       "Battery Power",         590, 1,    0,       "W",   "power",       "measurement",       true,  false},
        {"battery_current",     "Battery Current",       591, 0.01, 0,       "A",   "current",     "measurement",       true,  false},
        {"battery_temperature", "Battery Temperature",   586, 0.1,  -100.0f, "\xc2\xb0""C", "temperature", "measurement", false, false},
        {"battery_capacity_shutdown", "Battery Capacity Shutdown", 115, 1, 0, "%", "battery",     "measurement",       false, false},

        // Grid
        {"grid_power",        "Grid Power",        625, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_l1_power",     "Grid L1 Power",     622, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_l2_power",     "Grid L2 Power",     623, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_l3_power",     "Grid L3 Power",     624, 1,    0, "W",  "power",   "measurement", true,  false},
        {"grid_voltage_l1",   "Grid L1 Voltage",   598, 0.1,  0, "V",  "voltage", "measurement", false, false},
        {"grid_voltage_l2",   "Grid L2 Voltage",   599, 0.1,  0, "V",  "voltage", "measurement", false, false},
        {"grid_voltage_l3",   "Grid L3 Voltage",   600, 0.1,  0, "V",  "voltage", "measurement", false, false},
        {"grid_current_l1",   "Grid L1 Current",   610, 0.01, 0, "A",  "current", "measurement", true,  false},
        {"grid_current_l2",   "Grid L2 Current",   611, 0.01, 0, "A",  "current", "measurement", true,  false},
        {"grid_current_l3",   "Grid L3 Current",   612, 0.01, 0, "A",  "current", "measurement", true,  false},
        {"grid_frequency",    "Grid Frequency",    609, 0.01, 0, "Hz", nullptr,   "measurement", false, false},
        {"ct_power",          "Grid CT Power",     619, 1,    0, "W",  "power",   "measurement", true,  false},

        // Inverter
        {"inverter_power",     "Inverter Power",     636, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_l1_power",  "Inverter L1 Power",  633, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_l2_power",  "Inverter L2 Power",  634, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_l3_power",  "Inverter L3 Power",  635, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"inverter_l1_voltage", "Inverter L1 Voltage", 627, 0.1,  0,    "V",  "voltage", "measurement", false, false},
        {"inverter_l2_voltage", "Inverter L2 Voltage", 628, 0.1,  0,    "V",  "voltage", "measurement", false, false},
        {"inverter_l3_voltage", "Inverter L3 Voltage", 629, 0.1,  0,    "V",  "voltage", "measurement", false, false},
        {"inverter_frequency", "Inverter Frequency", 638, 0.01, 0,     "Hz", nullptr,   "measurement", false, false},

        // Load / gen
        {"load_power",         "Load Power",         653, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"load_l1_power",      "Load L1 Power",      650, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"load_l2_power",      "Load L2 Power",      651, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"load_l3_power",      "Load L3 Power",      652, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"gen_power",          "Gen Port Power",     667, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"gen_l1_power",       "Gen L1 Power",       664, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"gen_l2_power",       "Gen L2 Power",       665, 1,    0,     "W",  "power",   "measurement", true,  false},
        {"gen_l3_power",       "Gen L3 Power",       666, 1,    0,     "W",  "power",   "measurement", true,  false},

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

        // Temperatures: (raw - 1000) * 0.1 like the proven ESPHome config
        {"dc_transformer_temperature", "DC Transformer Temperature", 540, 0.1, -100.0f, "\xc2\xb0""C", "temperature", "measurement", true, false},
        {"radiator_temperature",       "Radiator Temperature",       541, 0.1, -100.0f, "\xc2\xb0""C", "temperature", "measurement", true, false},

        // Settings (raw system mode times, HHMM encoded)
        {"system_mode_time1", "System Mode Time 1", 148, 1, 0, nullptr, nullptr, nullptr, false, false},
        {"system_mode_time2", "System Mode Time 2", 149, 1, 0, nullptr, nullptr, nullptr, false, false},
        {"system_mode_time3", "System Mode Time 3", 150, 1, 0, nullptr, nullptr, nullptr, false, false},
        {"system_mode_time4", "System Mode Time 4", 151, 1, 0, nullptr, nullptr, nullptr, false, false},
        {"system_mode_time5", "System Mode Time 5", 152, 1, 0, nullptr, nullptr, nullptr, false, false},
        {"system_mode_time6", "System Mode Time 6", 153, 1, 0, nullptr, nullptr, nullptr, false, false},
    };

    constexpr size_t kSensorCount = sizeof(kSensors) / sizeof(kSensors[0]);

    // ---- Binary sensors ---------------------------------------------------------

    constexpr BinaryEntity kBinaries[] = {
        {"grid_connected", "Grid Connected Status", 552},
    };

    constexpr size_t kBinaryCount = sizeof(kBinaries) / sizeof(kBinaries[0]);

    // ---- Text sensors (computed) ---------------------------------------------

    constexpr TextEntity kTimeSlots[] = {
        {"time_slot_1", "Time Slot 1", 148},
        {"time_slot_2", "Time Slot 2", 149},
        {"time_slot_3", "Time Slot 3", 150},
        {"time_slot_4", "Time Slot 4", 151},
        {"time_slot_5", "Time Slot 5", 152},
        {"time_slot_6", "Time Slot 6", 153},
    };

    constexpr size_t kTimeSlotCount = sizeof(kTimeSlots) / sizeof(kTimeSlots[0]);

    // ---- Selects ----------------------------------------------------------------

    constexpr SelectOption kEnergyPatternOptions[] = {
        {"Battery first", 0},
        {"Load first", 1},
    };

    constexpr SelectOption kWorkModeOptions[] = {
        {"Selling First", 0},
        {"Zero Export + Limit to Load Only", 1},
        {"Limited to Home", 2},
    };

    // ---- Controls (numbers, switches, selects) -----------------------------------

    constexpr ControlEntity kControls[] = {
        // Numbers
        {"zero_export",                        "Zero Export",                  104, 1,   "W",  -20,   20,    1,   false, 0, 0, false, nullptr, 0, true,  0xFFFF, "mdi:transmission-tower-off"},
        {"battery_max_charge_current",         "Battery Max Charge Current",   108, 1,   "A",  0,     210,   5,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-arrow-up"},
        {"battery_max_discharge_current",      "Battery Max Discharge Current",109, 1,   "A",  0,     210,   5,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-arrow-down"},
        {"grid_charge_start_soc",              "Grid Charge Start SOC",        127, 1,   "%",  0,     100,   1,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-medium"},
        {"grid_charge_current",                "Grid Charge Current",          128, 1,   "A",  0,     210,   5,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging"},
        {"max_sell_power",                     "Max Sell Power",               143, 1,   "W",  0,     12000, 100, false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:solar-power"},
        {"system_mode_power_time1",            "System Mode Power Time 1",     154, 1,   "W",  0,     12000, 100, false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:clock-outline"},
        {"system_mode_power_time2",            "System Mode Power Time 2",     155, 1,   "W",  0,     12000, 100, false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:clock-outline"},
        {"system_mode_power_time3",            "System Mode Power Time 3",     156, 1,   "W",  0,     12000, 100, false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:clock-outline"},
        {"system_mode_power_time4",            "System Mode Power Time 4",     157, 1,   "W",  0,     12000, 100, false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:clock-outline"},
        {"system_mode_power_time5",            "System Mode Power Time 5",     158, 1,   "W",  0,     12000, 100, false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:clock-outline"},
        {"system_mode_power_time6",            "System Mode Power Time 6",     159, 1,   "W",  0,     12000, 100, false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:clock-outline"},
        {"system_mode_v_time1",                "System Mode V Time 1",         160, 1,   "V",  4320,  5760,  10,  false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-100"},
        {"system_mode_v_time2",                "System Mode V Time 2",         161, 1,   "V",  4320,  5760,  10,  false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-100"},
        {"system_mode_v_time3",                "System Mode V Time 3",         162, 1,   "V",  4320,  5760,  10,  false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-100"},
        {"system_mode_v_time4",                "System Mode V Time 4",         163, 1,   "V",  4320,  5760,  10,  false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-100"},
        {"system_mode_v_time5",                "System Mode V Time 5",         164, 1,   "V",  4320,  5760,  10,  false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-100"},
        {"system_mode_v_time6",                "System Mode V Time 6",         165, 1,   "V",  4320,  5760,  10,  false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-100"},
        {"system_mode_soc_time1",              "System Mode SOC Time 1",       166, 1,   "%",  0,     100,   1,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-medium"},
        {"system_mode_soc_time2",              "System Mode SOC Time 2",       167, 1,   "%",  0,     100,   1,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-medium"},
        {"system_mode_soc_time3",              "System Mode SOC Time 3",       168, 1,   "%",  0,     100,   1,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-medium"},
        {"system_mode_soc_time4",              "System Mode SOC Time 4",       169, 1,   "%",  0,     100,   1,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-medium"},
        {"system_mode_soc_time5",              "System Mode SOC Time 5",       170, 1,   "%",  0,     100,   1,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-medium"},
        {"system_mode_soc_time6",              "System Mode SOC Time 6",       171, 1,   "%",  0,     100,   1,   false, 0, 0, false, nullptr, 0, false, 0xFFFF, "mdi:battery-charging-medium"},

        // Switches (masked bit writes, see clearMask)
        {"toggle_on_off",       "Inverter ON/OFF",       80,  1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:power"},
        {"solar_sell",          "Solar Sell",            145, 1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:transmission-tower-export"},
        {"time_of_use",         "Time of Use",           146, 1, nullptr, 0, 1, 1, true, 0x00FF, 0, false, nullptr, 0, false, 0x00FF, "mdi:clock-outline"},
        {"grid_charge_time1",   "Grid Charge Time 1",    172, 1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:clock-outline"},
        {"grid_charge_time2",   "Grid Charge Time 2",    173, 1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:clock-outline"},
        {"grid_charge_time3",   "Grid Charge Time 3",    174, 1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:clock-outline"},
        {"grid_charge_time4",   "Grid Charge Time 4",    175, 1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:clock-outline"},
        {"grid_charge_time5",   "Grid Charge Time 5",    176, 1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:clock-outline"},
        {"grid_charge_time6",   "Grid Charge Time 6",    177, 1, nullptr, 0, 1, 1, true, 1,     0, false, nullptr, 0, false, 0x0001, "mdi:clock-outline"},

        // Selects
        {"energy_pattern", "Energy Pattern", 141, 1, nullptr, 0, 1, 1, false, 0, 0, true, kEnergyPatternOptions, 2, false, 0xFFFF, "mdi:swap-horizontal"},
        {"work_mode",      "Work Mode",      142, 1, nullptr, 0, 2, 1, false, 0, 0, true, kWorkModeOptions, 3, false, 0xFFFF, "mdi:cog"},
    };

    constexpr size_t kControlCount = sizeof(kControls) / sizeof(kControls[0]);

    // ---- Helpers -----------------------------------------------------------------

    bool IsWritable(uint16_t address);
    const char *OverallStateName(uint16_t raw);
}
