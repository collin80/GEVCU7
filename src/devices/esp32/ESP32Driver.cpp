#include "ESP32Driver.h"
#include "gevcu_port.h"

ESP32Driver::ESP32Driver() : Device()
{
    commonName = "ESP32 Wifi/BT Module";
    shortName = "ESP32";
}

void ESP32Driver::earlyInit()
{
    prefsHandler = new PrefHandler(ESP32);
}

void ESP32Driver::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: ESP32 Module (id:%X, %X)", ESP32, this);

    loadConfiguration();

    Device::setup(); // run the parent class version of this function

    Serial2.begin(115200);
    Serial2.setTimeout(2);

    pinMode(ESP32_ENABLE, OUTPUT);
    pinMode(ESP32_BOOT, OUTPUT);
    digitalWrite(ESP32_ENABLE, LOW); //start in reset
    digitalWrite(ESP32_BOOT, HIGH); //use normal mode not bootloader mode (bootloader is active low)
    delay(20);
    digitalWrite(ESP32_ENABLE, HIGH); //now bring it out of reset.

    tickHandler.attach(this, 5000);
}

void ESP32Driver::handleTick() {

    Device::handleTick(); //kick the ball up to papa
    while (Serial2.available())
    {
        char c = Serial2.read();
        if (c == '\n')
        {
            Logger::debug("ESP32: %s", bufferedLine.c_str());
            bufferedLine = "";
        }
        else bufferedLine += c;
    }
}

DeviceId ESP32Driver::getId() {
    return (ESP32);
}

uint32_t ESP32Driver::getTickInterval()
{
    return 5000;
}

void ESP32Driver::loadConfiguration() {
    /*
    CodaMotorControllerConfiguration *config = (CodaMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new CodaMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent
*/
}

void ESP32Driver::saveConfiguration() {
    Device::saveConfiguration();
}

ESP32Driver esp32Driver;