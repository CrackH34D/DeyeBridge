#include "Watchdog.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include "Logging.h"

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
                    Log::error("Watchdog: main loop silent for %lu ms, restarting",
                               static_cast<unsigned long>(sTimeoutMs));
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

        void LogBootReason()
        {
            switch (esp_reset_reason())
            {
                case ESP_RST_TASK_WDT:
                    Log::error("Boot: last reset was a task watchdog timeout");
                    break;
                case ESP_RST_INT_WDT:
                    Log::error("Boot: last reset was an interrupt watchdog");
                    break;
                case ESP_RST_WDT:
                    Log::error("Boot: last reset was another watchdog");
                    break;
                case ESP_RST_PANIC:
                    Log::error("Boot: last reset was a panic/crash");
                    break;
                case ESP_RST_BROWNOUT:
                    Log::warn("Boot: last reset was a brownout");
                    break;
                case ESP_RST_SW:
                    Log::info("Boot: last reset was a software restart");
                    break;
                case ESP_RST_POWERON:
                    Log::info("Boot: power-on reset");
                    break;
                default:
                    Log::info("Boot: reset reason %d", static_cast<int>(esp_reset_reason()));
                    break;
            }
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
