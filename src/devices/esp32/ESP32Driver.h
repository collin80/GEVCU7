#include <Arduino.h>
#include "../Device.h"
#include "../DeviceTypes.h"
#include "../../TickHandler.h"
#include "SerialFileSender.h"
#include <ArduinoJson.h>

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
};

class ESP32Configuration: public DeviceConfiguration {
public:
    uint8_t ssid[64];
    uint8_t ssid_pw[64];
    uint8_t hostName[64];
    uint8_t esp32_mode;
    uint8_t debugMode;
};

class ESP32Driver : public Device
{
public:
    virtual void handleTick();
    virtual void handleMessage(uint32_t msgType, const void* message);
    virtual void setup();
    void disableDevice();
    ESP32Driver();
    void processSerial();
    void sendLogString(String str);
    void sendStatusCSV(String str);

    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();
private:
    void sendWirelessConfig();
    void sendDeviceList();
    void sendDeviceDetails(uint16_t deviceID);
    void processConfigReply(JsonDocument* doc);
    void packageAndSendEntry(StatusEntry *entry);

    String bufferedLine;
    ESP32NS::ESP32_STATE currState;
    ESP32NS::ESP32_STATE desiredState;
    bool systemAlive;
    bool systemEnabled;
    uint8_t serialReadBuffer[1024];
    uint8_t serialWriteBuffer[1024];
    DynamicJsonDocument *websocket_json;
    SerialFileSender *fileSender;
    bool didInitialStatus;
    int statusIdx;
    uint32_t lastTime;
};

