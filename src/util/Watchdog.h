#pragma once

#include <stdint.h>

// Software watchdog: an independent FreeRTOS task restarts the device when
// the main loop stops notifying. Feed checks can be suspended while the main
// loop does intentionally long work (OTA firmware upload).
namespace DeyeBridge
{
    namespace Watchdog
    {
        // Creates the watchdog task pinned to the opposite core
        void Start(uint32_t timeoutMs);

        // Called once per main loop iteration
        void NotifyLoopAlive();

        // Suspend/resume around intentionally long main-loop work (OTA)
        void Suspend();
        void Resume();
    }
}
