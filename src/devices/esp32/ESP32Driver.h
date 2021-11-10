#include <Arduino.h>
#include "../Device.h"
#include "../DeviceTypes.h"
#include "../../TickHandler.h"

#define ESP32 0x800
#define CFG_TICK_INTERVAL_WIFI                      200000

class ESP32Driver : public Device
{
public:
    virtual void handleTick();
    virtual void setup();
    void earlyInit();
    ESP32Driver();

    DeviceId getId();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();
private:
    String bufferedLine;
};

