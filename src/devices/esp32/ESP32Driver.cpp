#include "ESP32Driver.h"
#include "gevcu_port.h"

ESP32Driver::ESP32Driver() : Device()
{
    commonName = "ESP32 Wifi/BT Module";
    shortName = "ESP32";
}

void ESP32Driver::earlyInit()
{
    //prefsHandler = new PrefHandler(ESP32);
}

void ESP32Driver::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: ESP32 Module (id:%X, %X)", ESP32, this);

    loadConfiguration();

    Device::setup(); // run the parent class version of this function

    pinMode(ESP32_ENABLE, OUTPUT);
    pinMode(ESP32_BOOT, OUTPUT);
    digitalWrite(ESP32_ENABLE, LOW);
    digitalWrite(ESP32_BOOT, HIGH);
    Serial2.begin(115200);

    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CODAUQM);
}


void ESP32Driver::handleTick() {

    Device::handleTick(); //kick the ball up to papa

}

DeviceId ESP32Driver::getId() {
    return (ESP32);
}

uint32_t ESP32Driver::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CODAUQM;
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
