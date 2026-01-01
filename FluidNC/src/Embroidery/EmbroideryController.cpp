#include "EmbroideryController.h"

// We need access to the Stepping engine or Motor pins.
// For this proof of concept, we will include the necessary headers.
#include "../Stepping.h"
#include "../Machine/MachineConfig.h"
#include "../NutsBolts.h"

EmbroideryController* embroidery = nullptr;

EmbroideryController* EmbroideryController::getInstance() {
    if (!embroidery) {
        embroidery = new EmbroideryController();
    }
    return embroidery;
}

EmbroideryController::EmbroideryController() : _needleSensorPin("needle_sensor_pin") {
    // Constructor
}

void EmbroideryController::group(Configuration::HandlerBase& handler) {
    handler.item("needle_sensor_pin", _needleSensorPin);
}

volatile uint32_t EmbroideryController::_lastTriggerTime = 0;

void EmbroideryController::begin() {
    // Create a queue that can hold 50 stitches
    stitchQueue = xQueueCreate(50, sizeof(StitchCommand));
    
    // Create the worker task pinned to Core 1 (same as FluidNC)
    xTaskCreatePinnedToCore(
        taskFunction,       // Function
        "EmbroideryTask",   // Name
        4096,               // Stack size
        this,               // Param
        1,                  // Priority (Low priority, let system run)
        &_taskHandle,       // Handle
        1                   // Core
    );
    
    // Setup Pins
    if (_needleSensorPin.undefined()) {
        return;
    }
    
    _needleSensorPin.init();
    
    // Attach Interrupt
    attachInterrupt(_needleSensorPin.getNative(Pin::Capabilities::Input), onNeedleSyncInterrupt, FALLING); 
    
    // TEST MODE: Enable immediately
    enable(true);
    // Startup message removed as requested
}

void EmbroideryController::enable(bool state) {
    _enabled = state;
}

bool EmbroideryController::queueStitch(int32_t dx, int32_t dy, uint32_t zRpm) {
    StitchCommand cmd;
    cmd.stepsX = dx;
    cmd.stepsY = dy;
    cmd.zSpeedRPM = zRpm;
    
    // Send to queue, WAIT FOREVER if full. 
    // This blocks the G-code parser until the embroidery task consumes stitches.
    if (xQueueSend(stitchQueue, &cmd, portMAX_DELAY) == pdTRUE) {
        ets_printf("Stitch Queued: %d, %d\n", dx, dy); 
        return true;
    }
    return false;
}

// ---------------------------------------------------------
// THE INTERRUPT ROUTINE (The "Trigger")
// ---------------------------------------------------------
void IRAM_ATTR EmbroideryController::onNeedleSyncInterrupt() {
    // 1. Debounce (50ms)
    uint32_t now = millis();
    if (now - _lastTriggerTime < 50) {
        return;
    }
    _lastTriggerTime = now;

    if (!embroidery || !embroidery->_enabled) return;
    
    // 2. Notify the Task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(embroidery->_taskHandle, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// ---------------------------------------------------------
// THE WORKER TASK (The "Consumer")
// ---------------------------------------------------------
void EmbroideryController::taskFunction(void* pvParameters) {
    EmbroideryController* self = (EmbroideryController*)pvParameters;
    StitchCommand currentStitch;
    
    while (true) {
        // 1. Wait for Notification from ISR (Block indefinitely)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 2. Check Queue
        if (xQueueReceive(self->stitchQueue, &currentStitch, 0) == pdTRUE) {
            ets_printf("Executing Stitch: %d, %d\n", currentStitch.stepsX, currentStitch.stepsY);
            executeBurstMove(currentStitch);
        } else {
            ets_printf("Triggered, but Queue Empty!\n");
        }
    }
}

// ---------------------------------------------------------
// BURST MOVEMENT LOGIC
// ---------------------------------------------------------
void IRAM_ATTR EmbroideryController::executeBurstMove(StitchCommand cmd) {
    // Simple Bresenham-like loop to pulse X and Y
    // This runs in the TASK context, so delays are safe!
    
    int32_t dx = abs(cmd.stepsX);
    int32_t dy = abs(cmd.stepsY);
    
    // Direction Mask
    AxisMask dir_mask = 0;
    if (cmd.stepsX < 0) set_bitnum(dir_mask, X_AXIS); 
    if (cmd.stepsY < 0) set_bitnum(dir_mask, Y_AXIS);
    
    int32_t current_x = 0;
    int32_t current_y = 0;
    
    int32_t total_steps = (dx > dy) ? dx : dy;
    
    // Speed control
    uint32_t step_delay_us = 2000; 

    uint32_t start_time = micros();

    for (int i = 0; i < total_steps; i++) {
        AxisMask step_mask = 0;
        
        if (current_x < dx) {
            set_bitnum(step_mask, X_AXIS);
            current_x++;
        }
        if (current_y < dy) {
            set_bitnum(step_mask, Y_AXIS);
            current_y++;
        }
        
        Machine::Stepping::step(step_mask, dir_mask);
        ets_delay_us(5); 
        Machine::Stepping::unstep();
        
        ets_delay_us(step_delay_us);
    }
    
    uint32_t duration = micros() - start_time;
    ets_printf("Stitch Time: %lu us\n", duration);
}
