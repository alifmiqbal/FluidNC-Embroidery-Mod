#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "Configuration/Configurable.h"
#include "Machine/InputPin.h"

// Structure to hold a single stitch movement
struct StitchCommand {
    int32_t stepsX;
    int32_t stepsY;
    uint32_t zSpeedRPM; // Desired Z Speed for this stitch
};

class EmbroideryController : public Configuration::Configurable {
public:
    static EmbroideryController* getInstance();

    void begin();
    void group(Configuration::HandlerBase& handler) override;
    
    // Called by the Planner to add stitches
    bool queueStitch(int32_t dx, int32_t dy, uint32_t zRpm);

    // Enable/Disable the Embroidery Mode
    void enable(bool state);
    bool isEnabled() const { return _enabled; }

    // ISR for Needle Sensor (The Trigger)
    static void IRAM_ATTR onNeedleSyncInterrupt();

    EmbroideryController(); // Singleton, but public for Config System

private:

    // Queue handle for passing stitches from Planner to Executor
    QueueHandle_t stitchQueue;
    
    // Pins
    InputPin _needleSensorPin;
    
    bool _enabled = false;
    
    // The "Burst" movement function (runs in Task context)
    static void IRAM_ATTR executeBurstMove(StitchCommand cmd);
    
    // FreeRTOS Task
    TaskHandle_t _taskHandle = nullptr;
    static void taskFunction(void* pvParameters);
    
    // Debounce
    static volatile uint32_t _lastTriggerTime;
};

extern EmbroideryController* embroidery;
