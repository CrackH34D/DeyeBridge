#include "Watchdog.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

namespace DeyeBridge
{
    namespace Watchdog
    {
        static TaskHandle_t sTaskHandle = nullptr;
        static volatile uint32_t sTimeoutMs = 20000;
        static volatile bool sSuspended = false;

        static void WatchdogTask(void *)
        {
            for (;;)
            {
                const bool alive = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sTimeoutMs)) != 0;
                if (!alive && !sSuspended)
                {
                    delay(100);   // give the serial log a moment to flush
                    esp_restart();
                }
                // Suspended: simply wait again, a missed notification is fine
            }
        }

        void Start(uint32_t timeoutMs)
        {
            sTimeoutMs = timeoutMs;
            xTaskCreatePinnedToCore(WatchdogTask, "watchdog", 3072, nullptr, 1,
                                    &sTaskHandle, 0);
        }

        void NotifyLoopAlive()
        {
            if (sTaskHandle != nullptr)
            {
                xTaskNotify(sTaskHandle, 1, eSetValueWithOverwrite);
            }
        }

        void Suspend()
        {
            sSuspended = true;
        }

        void Resume()
        {
            sSuspended = false;
        }
    }
}
