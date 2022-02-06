#include <Arduino.h>
#include "../Device.h"
#include "../DeviceTypes.h"
#include "../../TickHandler.h"

#define ESP32 0x800
#define CFG_TICK_INTERVAL_WIFI                      200000

namespace ESP32NS
{
    enum ESP32_STATE
    {
        RESET,
        NORMAL,
        BOOTLOADER
    };
}

class ESP32Configuration: public DeviceConfiguration {
public:
    uint8_t ssid[64];
    uint8_t ssid_pw[64];
    uint8_t esp32_mode;
};

class ESP32Driver : public Device
{
public:
    virtual void handleTick();
    virtual void setup();
    void earlyInit();
    ESP32Driver();
    void processSerial();

    DeviceId getId();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();
private:
    void sendSSID(); //send SSID to ESP32
    void sendPW(); //send password / WPA2 key to esp32
    void sendESPMode(); //whether to be an AP or connect to existing SSID

    String bufferedLine;
    ESP32NS::ESP32_STATE currState;
    ESP32NS::ESP32_STATE desiredState;
    bool systemAlive;
    uint8_t serialReadBuffer[1024];
    uint8_t serialWriteBuffer[1024];
};

