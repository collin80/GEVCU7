#include <Arduino.h>
#include "../Device.h"
#include "../DeviceTypes.h"
#include "../../TickHandler.h"
#include "SerialFileSender.h"
#include <ArduinoJson.h>

#include <atomic>

#define ESP32 0x800
#define CFG_TICK_INTERVAL_WIFI                      200000
#define ESP32_REQ_QUEUE_SIZE                        8
#define ESP32_REQ_MAX_LEN                           300

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
    uint8_t firmwareFile[64];
    char ipAddr[40];
    int espUpdateType;
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
    void sendLogString(const char *str);
    void sendStatusCSV(const char *str);
    void doTeensyUpdate();
    void doESP32Update();
    bool isReady();
    void processQueuedRequests(); //called from loop() (thread 0)

    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();
private:
    void sendWirelessConfig();
    void sendDeviceList();
    void sendDeviceDetails(uint16_t deviceID);
    void sendStatusList();
    void processConfigReply(JsonDocument* doc);
    void packageAndSendEntry(StatusEntry *entry);
    void queueRequest(const String &line);
    bool sendJsonLine(const char *prefix, JsonDocument &doc, const char *suffix);

    String bufferedLine;
    //JSON requests from the ESP32 arrive in processSerial (inside yield(), on any thread) where no real work
    //may be done. They're copied into this ring and handled by handleTick. Single producer (processSerial)
    //and single consumer (handleTick) so the atomic head/tail indexes are enough. No lock needed.
    char reqQueue[ESP32_REQ_QUEUE_SIZE][ESP32_REQ_MAX_LEN + 1];
    std::atomic<uint8_t> reqHead;
    std::atomic<uint8_t> reqTail;
    //Taken around every write to Serial2 so a multi-KB JSON line can't get log lines spliced into it.
    //Sits between logMutex and consoleMutex in the lock order (Logger calls sendLogString holding logMutex).
    RecursiveMutex txMutex;
    ESP32NS::ESP32_STATE currState;
    ESP32NS::ESP32_STATE desiredState;
    bool systemAlive;
    bool systemEnabled;
    bool inhibitJSON;
    bool gotIPAddr;
    bool needWifiConfig;
    uint8_t serialReadBuffer[1024];
    uint8_t serialWriteBuffer[16384]; //large enough to hold a whole device config reply
    DynamicJsonDocument *websocket_json;
    //websocket_json is filled by handleMessage (called on the DeviceManager thread) and serialized/cleared
    //by handleTick on this device's own thread so both lock this around any use of it.
    RecursiveMutex jsonMutex;
    SerialFileSender *fileSender;
    bool didInitialStatus;
    int statusIdx;
    uint32_t lastTime;
    ESP32Configuration *config;
};

extern ESP32Driver esp32Driver;

