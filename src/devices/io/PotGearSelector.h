#pragma once

#include <Arduino.h>
#include "../../config.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"

#define POTGEARSEL 0x1038
#define TICK_POTGEAR    200000

class PotGearSelConfiguration: public DeviceConfiguration
{
public:
    uint8_t adcPin;
    uint16_t hysteresis;
    uint16_t parkPosition;
    uint16_t drivePosition;
    uint16_t reversePosition;
    uint16_t neutralPosition;
};

class PotGearSelector: public Device {
public:
    PotGearSelector();
    void setup();
    void handleTick();
    uint32_t getTickInterval();

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
};
