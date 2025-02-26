/*
 * BrusaMotorController.cpp
 *
 *
 Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#include "BrusaMotorController.h"

/*
 Warning:
 At high speed disable the DMC_EnableRq can be dangerous because a field weakening current is
 needed to achieve zero torque. Switching off the DMC in such a situation will make heavy regenerat-
 ing torque that can't be controlled.
 */

/*
Additional warning:
GEVCU7 has transitioned to using floating point for most things as the processor handles it natively.
This code has been roughly ported over but probably not properly. I have no inverter to test with.
So, this probably needs work.
*/

/*
 * Constructor
 */
BrusaMotorController::BrusaMotorController() : MotorController() {    
    torqueAvailable = 0;
    maxPositiveTorque = 0;
    minNegativeTorque = 0;
    limiterStateNumber = 0;

    tickCounter = 0;

    commonName = "Brusa DMC5 Inverter";
    shortName = "DMC5";
    deviceId = BRUSA_DMC5;
}

/*
 * Setup the device if it is enabled in configuration.
 */
void BrusaMotorController::setup() {
    tickHandler.detach(this);

    Logger::info("add device: Brusa DMC5 (id: %X, %X)", BRUSA_DMC5, this);

    loadConfiguration();
    MotorController::setup(); // run the parent class version of this function

    // register ourselves as observer of 0x258-0x268 and 0x458 can frames
    canHandlerIsolated.attach(this, CAN_MASKED_ID_1, CAN_MASK_1, false);
    canHandlerIsolated.attach(this, CAN_MASKED_ID_2, CAN_MASK_2, false);

    setAlive();

    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_BRUSA);
}

/*
 * Process event from the tick handler.
 *
 * The super-class requests desired levels from the throttle and
 * brake and decides which one to apply.
 */
void BrusaMotorController::handleTick() {
    MotorController::handleTick(); // call parent
    tickCounter++;

    checkAlive(1000);

    sendControl();	// send CTRL every 20ms
    if (tickCounter > 4) {
        sendControl2();	// send CTRL_2 every 100ms
        sendLimits();	// send LIMIT every 100ms
        tickCounter = 0;
    }
}

/*
 * Send DMC_CTRL message to the motor controller.
 *
 * This message controls the power-stage in the controller, clears the error latch
 * in case errors were detected and requests the desired torque / speed.
 */
void BrusaMotorController::sendControl() {
    BrusaMotorControllerConfiguration *config = (BrusaMotorControllerConfiguration *)getConfiguration();
    prepareOutputFrame(CAN_ID_CONTROL);

    speedRequested = 0;
    torqueRequested = 0;

    outputFrame.buf[0] = enablePositiveTorqueSpeed; // | enableNegativeTorqueSpeed;
    if (faulted) {
        outputFrame.buf[0] |= clearErrorLatch;
    } else {
        if ((running || speedActual > 1000) && !systemIO.getDigitalIn(1)) { // see warning about field weakening current to prevent uncontrollable regen
            outputFrame.buf[0] |= enablePowerStage;
        }
        if (running) {
            if (config->enableOscillationLimiter)
                outputFrame.buf[0] |= enableOscillationLimiter;

            if (powerMode == modeSpeed) {
                outputFrame.buf[0] |= enableSpeedMode;
                speedRequested = throttleRequested * config->speedMax / 1000;
                torqueRequested = config->torqueMax; // positive number used for both speed directions
            } else { // torque mode
                speedRequested = config->speedMax; // positive number used for both torque directions
                torqueRequested = throttleRequested * config->torqueMax / 1000;
            }

            // set the speed in rpm
            outputFrame.buf[2] = (speedRequested & 0xFF00) >> 8;
            outputFrame.buf[3] = (speedRequested & 0x00FF);

            // set the torque in 0.01Nm (GEVCU uses Nm -> multiply by 100)
            int16_t torq = torqueRequested * 100;
            outputFrame.buf[4] = (torq & 0xFF00) >> 8;
            outputFrame.buf[5] = (torq & 0x00FF);
        }
    }

    if (Logger::isDebug())
        Logger::debug(BRUSA_DMC5, "requested Speed: %i rpm, requested Torque: %.2f Nm", speedRequested, (float)torqueRequested);

    canHandlerIsolated.sendFrame(outputFrame);
}

/*
 * Send DMC_CTRL2 message to the motor controller.
 *
 * This message controls the mechanical power limits for motor- and regen-mode.
 */
void BrusaMotorController::sendControl2() {
    BrusaMotorControllerConfiguration *config = (BrusaMotorControllerConfiguration *)getConfiguration();

    prepareOutputFrame(CAN_ID_CONTROL_2);
    uint16_t torqSlew = (config->torqueSlewRate * 100);
    outputFrame.buf[0] = (torqSlew & 0xFF00) >> 8;
    outputFrame.buf[1] = (torqSlew & 0x00FF);
    outputFrame.buf[2] = (config->speedSlewRate & 0xFF00) >> 8;
    outputFrame.buf[3] = (config->speedSlewRate & 0x00FF);
    outputFrame.buf[4] = (config->maxMechanicalPowerMotor & 0xFF00) >> 8;
    outputFrame.buf[5] = (config->maxMechanicalPowerMotor & 0x00FF);
    outputFrame.buf[6] = (config->maxMechanicalPowerRegen & 0xFF00) >> 8;
    outputFrame.buf[7] = (config->maxMechanicalPowerRegen & 0x00FF);

    canHandlerIsolated.sendFrame(outputFrame);
}

/*
 * Send DMC_LIM message to the motor controller.
 *
 * This message controls the electrical limits in the controller.
 */
void BrusaMotorController::sendLimits() {
    BrusaMotorControllerConfiguration *config = (BrusaMotorControllerConfiguration *)getConfiguration();

    prepareOutputFrame(CAN_ID_LIMIT);
    uint16_t dcVoltLim = config->dcVoltLimitMotor;
    outputFrame.buf[0] = (dcVoltLim & 0xFF00) >> 8;
    outputFrame.buf[1] = (dcVoltLim & 0x00FF);

    uint16_t dcVoltLimR = config->dcVoltLimitRegen;
    outputFrame.buf[2] = (dcVoltLimR & 0xFF00) >> 8;
    outputFrame.buf[3] = (dcVoltLimR & 0x00FF);
    uint16_t dcCurrLimM = config->dcCurrentLimitMotor;
    uint16_t dcCurrLimR = config->dcCurrentLimitRegen;
    outputFrame.buf[4] = (dcCurrLimM & 0xFF00) >> 8;
    outputFrame.buf[5] = (dcCurrLimM & 0x00FF);
    outputFrame.buf[6] = (dcCurrLimR & 0xFF00) >> 8;
    outputFrame.buf[7] = (dcCurrLimR & 0x00FF);

    canHandlerIsolated.sendFrame(outputFrame);
}

/*
 * Prepare the CAN transmit frame.
 * Re-sets all parameters in the re-used frame.
 */
void BrusaMotorController::prepareOutputFrame(uint32_t id) {
    outputFrame.len = 8;
    outputFrame.id = id;
    outputFrame.flags.extended = 0;

    outputFrame.buf[1] = 0;
    outputFrame.buf[2] = 0;
    outputFrame.buf[3] = 0;
    outputFrame.buf[4] = 0;
    outputFrame.buf[5] = 0;
    outputFrame.buf[6] = 0;
    outputFrame.buf[7] = 0;
}

/*
 * Processes an event from the CanHandler.
 *
 * In case a CAN message was received which pass the masking and id filtering,
 * this method is called. Depending on the ID of the CAN message, the data of
 * the incoming message is processed.
 */
void BrusaMotorController::handleCanFrame( const CAN_message_t &frame) {
    setAlive();
    switch (frame.id) {
    case CAN_ID_STATUS:
        processStatus(frame.buf);
        break;
    case CAN_ID_ACTUAL_VALUES:
        processActualValues(frame.buf);
        break;
    case CAN_ID_ERRORS:
        processErrors(frame.buf);
        break;
    case CAN_ID_TORQUE_LIMIT:
        processTorqueLimit(frame.buf);
        break;
    case CAN_ID_TEMP:
        processTemperature(frame.buf);
        break;
    default:
        Logger::warn(BRUSA_DMC5, "received unknown frame id %X", frame.id);
    }
}

/*
 * Process a DMC_TRQS message which was received from the motor controller.
 *
 * This message provides the general status of the controller as well as
 * available and current torque and speed.
 */
void BrusaMotorController::processStatus(const uint8_t data[]) {
    uint32_t brusaStatus = (uint32_t)(data[1] | (data[0] << 8));
    torqueAvailable = (int16_t)(data[3] | (data[2] << 8)) / 100.0f;
    torqueActual = (int16_t)(data[5] | (data[4] << 8)) / 100.0f;
    speedActual = (int16_t)(data[7] | (data[6] << 8));

    if(Logger::isDebug())
        Logger::debug(BRUSA_DMC5, "status: %X, torque avail: %.2fNm, actual torque: %.2fNm, speed actual: %urpm", brusaStatus, (float)torqueAvailable/100.0F, (float)torqueActual/100.0F, speedActual);

    ready = (brusaStatus & stateReady) != 0 ? true : false;
    running = (brusaStatus & stateRunning) != 0 ? true : false;
    faulted = (brusaStatus & errorFlag) != 0 ? true : false;
    warning = (brusaStatus & warningFlag) != 0 ? true : false;
}

/*
 * Process a DMC_ACTV message which was received from the motor controller.
 *
 * This message provides information about current electrical conditions and
 * applied mechanical power.
 */
void BrusaMotorController::processActualValues(const uint8_t data[]) {
    dcVoltage = (data[1] | (data[0] << 8));
    dcCurrent = (int16_t)(data[3] | (data[2] << 8)) / 1.0f;
    acCurrent = (data[5] | (data[4] << 8)) / 2.5f;
    mechanicalPower = (int16_t)(data[7] | (data[6] << 8)) / 6.25f;

    if (Logger::isDebug())
        Logger::debug(BRUSA_DMC5, "actual values: DC Volts: %.1fV, DC current: %.1fA, AC current: %fA, mechPower: %fkW", (float)dcVoltage, (float)dcCurrent, (float)acCurrent, (float)mechanicalPower);
}

/*
 * Process a DMC_ERRS message which was received from the motor controller.
 *
 * This message provides various error and warning status flags in a bitfield.
 * The bitfield is not processed here but it is made available for other components
 * (e.g. the webserver to display the various status flags)
 */
void BrusaMotorController::processErrors(const uint8_t data[]) {
    //statusBitfield3 = (uint32_t)(data[1] | (data[0] << 8) | (data[5] << 16) | (data[4] << 24));
    //statusBitfield2 = (uint32_t)(data[7] | (data[6] << 8));

    //if (Logger::isDebug())
    //    Logger::debug(BRUSA_DMC5, "errors: %X, warning: %X", statusBitfield3, statusBitfield2);
}

/*
 * Process a DMC_TRQS2 message which was received from the motor controller.
 *
 * This message provides information about available torque.
 */
void BrusaMotorController::processTorqueLimit(const uint8_t data[]) {
    maxPositiveTorque = (int16_t)(data[1] | (data[0] << 8)) / 100.0f;
    minNegativeTorque = (int16_t)(data[3] | (data[2] << 8)) / 100.0f;
    limiterStateNumber = (uint8_t)data[4];

    if (Logger::isDebug())
        Logger::debug(BRUSA_DMC5, "torque limit: max positive: %fNm, min negative: %fNm", (float) maxPositiveTorque, (float) minNegativeTorque, limiterStateNumber);
}

/*
 * Process a DMC_TEMP message which was received from the motor controller.
 *
 * This message provides information about motor and inverter temperatures.
 */
void BrusaMotorController::processTemperature(const uint8_t data[]) {
    temperatureInverter = (int16_t)(data[1] | (data[0] << 8)) * 0.5f;
    temperatureMotor = (int16_t)(data[3] | (data[2] << 8)) * 0.5f;
    temperatureSystem = (int16_t)(data[4] - 50) * 1.0f;

    if (Logger::isDebug())
        Logger::debug(BRUSA_DMC5, "temperature: inverter: %fC, motor: %fC, system: %fC", (float)temperatureInverter, (float)temperatureMotor, (float)temperatureSystem);
}

/*
 * Expose the tick interval of this controller
 */
uint32_t BrusaMotorController::getTickInterval() {
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER_BRUSA;
}

/*
 * Load configuration data from EEPROM.
 *
 * If not available or the checksum is invalid, default values are chosen.
 */
void BrusaMotorController::loadConfiguration() {
    BrusaMotorControllerConfiguration *config = (BrusaMotorControllerConfiguration *)getConfiguration();

    if(!config) { // as lowest sub-class make sure we have a config object
        config = new BrusaMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent


//	if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
//        Logger::debug(BRUSA_DMC5, (char *)Constants::validChecksum);
//		prefsHandler->read(EEMC_, &config->minimumLevel1);
//    } else { //checksum invalid. Reinitialize values and store to EEPROM
//        Logger::warn(BRUSA_DMC5, (char *)Constants::invalidChecksum);
        prefsHandler->read("maxMecPowerMotor", &config->maxMechanicalPowerMotor, 50000);
        prefsHandler->read("maxMecPowerRegen", &config->maxMechanicalPowerRegen, 0);
        prefsHandler->read("dcVoltLimMotor", &config->dcVoltLimitMotor, 1000);
        prefsHandler->read("dcVoltLimRegen", &config->dcVoltLimitRegen, 0);
        prefsHandler->read("dcCurrLimMotor", &config->dcCurrentLimitMotor, 0);
        prefsHandler->read("dcCurrLimRegen", &config->dcCurrentLimitRegen, 0);
        prefsHandler->read("enableOscLim", (uint8_t *)&config->enableOscillationLimiter, (uint8_t)0);
    //}
    Logger::debug(BRUSA_DMC5, "Max mech power motor: %f kW, max mech power regen: %f ", config->maxMechanicalPowerMotor, config->maxMechanicalPowerRegen);
    Logger::debug(BRUSA_DMC5, "DC limit motor: %f Volt, DC limit regen: %f Volt", config->dcVoltLimitMotor, config->dcVoltLimitRegen);
    Logger::debug(BRUSA_DMC5, "DC limit motor: %f Amps, DC limit regen: %f Amps", config->dcCurrentLimitMotor, config->dcCurrentLimitRegen);
}

/*
 * Store the current configuration parameters to EEPROM.
 */
void BrusaMotorController::saveConfiguration() {
    BrusaMotorControllerConfiguration *config = (BrusaMotorControllerConfiguration *)getConfiguration();

    MotorController::saveConfiguration(); // call parent
    prefsHandler->write("maxMecPowerMotor", config->maxMechanicalPowerMotor);
	prefsHandler->write("maxMecPowerRegen", config->maxMechanicalPowerRegen);
	prefsHandler->write("dcVoltLimMotor", config->dcVoltLimitMotor);
	prefsHandler->write("dcVoltLimRegen", config->dcVoltLimitRegen);
	prefsHandler->write("dcCurrLimMotor", config->dcCurrentLimitMotor);
	prefsHandler->write("dcCurrLimRegen", config->dcCurrentLimitRegen);
	prefsHandler->write("enableOscLim", (uint8_t)config->enableOscillationLimiter);
    prefsHandler->saveChecksum();
}

DMAMEM BrusaMotorController brusaMC;
