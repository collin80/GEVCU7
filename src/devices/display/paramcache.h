#ifndef PARAMCACHE_H_
#define PARAMCACHE_H_

/**
 * Cache of param values to avoid sending an update unless changed
 */
struct ParamCache {
    uint32_t timeRunning;
    int16_t torqueRequested;
    int16_t torqueActual;
    int16_t throttle;
    int16_t brake;
    bool brakeNotAvailable;
    int16_t speedRequested;
    int16_t speedActual;
    MotorController::PowerMode powerMode;
    MotorController::Gears gear;
    int16_t dcVoltage;
    int16_t dcCurrent;
    int16_t acCurrent;
    int16_t nominalVolt;
    int16_t kiloWattHours;
    uint32_t bitfield1;
    uint32_t bitfield2;
    uint32_t bitfield3;
    uint32_t bitfield4;
    bool running;
    bool faulted;
    bool warning;
    int16_t tempMotor;
    int16_t tempInverter;
    int16_t tempSystem;
    int16_t mechPower;
    int16_t prechargeR;
    int8_t prechargeRelay;
    int8_t mainContactorRelay;
    int8_t coolFan;
    int8_t coolOn;
    int8_t coolOff;
    int8_t brakeLight;
    int8_t revLight;
    int8_t enableIn;
    int8_t reverseIn;
};

#endif