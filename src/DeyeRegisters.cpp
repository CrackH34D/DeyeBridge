#include "DeyeRegisters.h"

namespace DeyeRegisters
{
    bool IsWritable(uint16_t address)
    {
        for (size_t i = 0; i < kWritableRegisterCount; ++i)
        {
            if (kWritableRegisters[i] == address)
            {
                return true;
            }
        }
        return false;
    }

    const char *OverallStateName(uint16_t raw)
    {
        switch (raw)
        {
            case 0: return "standby";
            case 1: return "selftest";
            case 2: return "normal";
            case 3: return "alarm";
            case 4: return "fault";
            default: return "unknown";
        }
    }
}
