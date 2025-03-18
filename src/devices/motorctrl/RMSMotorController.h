/*
 * RMSMotorController.h
 *
 *
 Copyright (c) 2017 Collin Kidder

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

#ifndef RMSCTRL_H_
#define RMSCTRL_H_

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

#define RINEHARTINV 0x1004

//RMS inverters have very detailed fault output so we'll support them all custom here.
enum RMS_FAULTS
{
    RMS_POST_DESAT = 2000,
    RMS_POST_OVERCURR,
    RMS_POST_ACCEL_SHORTED,
    RMS_POST_ACCEL_OPEN,
    RMS_POST_CURR_LOW,
    RMS_POST_CURR_HIGH,
    RMS_POST_MOD_TEMP_LOW,
    RMS_POST_MOD_TEMP_HIGH,
    RMS_POST_PCB_TEMP_LOW,
    RMS_POST_PCB_TEMP_HIGH,
    RMS_POST_GATEDRV_TEMP_LOW,
    RMS_POST_GATEDRV_TEMP_HIGH,
    RMS_POST_5V_LOW,
    RMS_POST_5V_HIGH,
    RMS_POST_12V_LOW,
    RMS_POST_12V_HIGH,
    RMS_POST_25V_LOW,
    RMS_POST_25V_HIGH,
    RMS_POST_15V_LOW,
    RMS_POST_15V_HIGH,
    RMS_POST_HVDC_HIGH,
    RMS_POST_HVDC_LOW,
    RMS_POST_PRECHARGE_TIMEOUT,
    RMS_POST_PRECHARGE_FAILURE,
    RMS_POST_EEPROM_CHECKSUM,
    RMS_POST_EEPROM_CORRUPT,
    RMS_POST_EEPROM_UPDATE,
    RMS_POST_BRAKE_SHORTED,
    RMS_POST_BRAKE_OPEN,
    RMS_RUN_MOTOR_OVERSPEED,
    RMS_RUN_OVERCURR,
    RMS_RUN_OVERVOLT,
    RMS_RUN_INV_OVERTEMP,
    RMS_RUN_ACCEL_SHORTED,
    RMS_RUN_ACCEL_OPEN,
    RMS_RUN_DIRCMD,
    RMS_RUN_INV_RESPONSE_TIMEOUT,
    RMS_RUN_HWDESAT,
    RMS_RUN_HWOVERCURR,
    RMS_RUN_UNDERVOLT,
    RMS_RUN_COMM_LOST,
    RMS_RUN_MOTOR_OVERTEMP,
    RMS_RUN_BRAKE_SHORTED,
    RMS_RUN_BRAKE_OPEN,
    RMS_RUN_IGBTA_OVERTEMP,
    RMS_RUN_IGBTB_OVERTEMP,
    RMS_RUN_IGBTC_OVERTEMP,
    RMS_RUN_PCB_OVERTEMP,
    RMS_RUN_GATE1_OVERTEMP,
    RMS_RUN_GATE2_OVERTEMP,
    RMS_RUN_GATE3_OVERTEMP,
    RMS_RUN_CURR_SENSE_FAULT,
    RMS_RUN_RESOLVER_MISSING,
    RMS_RUN_INV_DISCHARGE,
    RMS_LAST_FAULT
};

static const char* RMS_FAULT_DESCS[] =
{
    "POST - Desat Fault!",
    "POST - HW Over Current Limit!",
    "POST - Accelerator Shorted!",
    "POST - Accelerator Open!",
    "POST - Current Sensor Low!",
    "POST - Current Sensor High!",
    "POST - Module Temperature Low!",
    "POST - Module Temperature High!",
    "POST - Control PCB Low Temp!",
    "POST - Control PCB High Temp!",
    "POST - Gate Drv PCB Low Temp!",
    "POST - Gate Drv PCB High Temp!",
    "POST - 5V Voltage Low!",
    "POST - 5V Voltage High!",
    "POST - 12V Voltage Low!",
    "POST - 12V Voltage High!",
    "POST - 2.5V Voltage Low!",
    "POST - 2.5V Voltage High!",
    "POST - 1.5V Voltage Low!",
    "POST - 1.5V Voltage High!",
    "POST - DC Bus Voltage High!",
    "POST - DC Bus Voltage Low!",
    "POST - Precharge Timeout!",
    "POST - Precharge Voltage Failure!",
    "POST - EEPROM Checksum Invalid!",
    "POST - EEPROM Data Out of Range!",
    "POST - EEPROM Update Required!",
    "POST - Brake Shorted!",
    "POST - Brake Open!",
    "Motor Over Speed!",
    "Over Current!",
    "Over Voltage!",
    "Inverter Over Temp!",
    "Accelerator Shorted!",
    "Accelerator Open!",
    "Direction Cmd Fault!",
    "Inverter Response Timeout!",
    "Hardware Desat Error!",
    "Hardware Overcurrent Fault!",
    "Under Voltage!",
    "CAN Cmd Message Lost!",
    "Motor Over Temperature!",
    "Brake Input Shorted!",
    "Brake Input Open!",
    "IGBT A Over Temperature!",
    "IGBT B Over Temperature!",
    "IGBT C Over Temperature!",
    "PCB Over Temperature!",
    "Gate Drive 1 Over Temperature!",
    "Gate Drive 2 Over Temperature!",
    "Gate Drive 3 Over Temperature!",
    "Current Sensor Fault!",
    "Resolver Not Connected!",
    "Inverter Discharge Active!",
};

/*
 * Class for Rinehart PM Motor Controller specific configuration parameters
 */
class RMSMotorControllerConfiguration : public MotorControllerConfiguration {
public:
    uint8_t canbusNum;
};

class RMSMotorController: public MotorController, CanObserver {

public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();

    RMSMotorController();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();
    virtual const char* getFaultDescription(uint16_t faultcode);

private:
    byte alive;
    byte sequence;
    uint16_t torqueCommand;
	uint32_t mss;
	bool isLockedOut;
	bool isEnabled;
	bool isCANControlled;
    RMSMotorControllerConfiguration *config;

   void sendCmdFrame();
   void handleCANMsgTemperature1(uint8_t *data);
   void handleCANMsgTemperature2(uint8_t *data);
   void handleCANMsgTemperature3(uint8_t *data);
   void handleCANMsgAnalogInputs(uint8_t *data);
   void handleCANMsgDigitalInputs(uint8_t *data);
   void handleCANMsgMotorPos(uint8_t *data);
   void handleCANMsgCurrent(uint8_t *data);
   void handleCANMsgVoltage(uint8_t *data);
   void handleCANMsgFlux(uint8_t *data);
   void handleCANMsgIntVolt(uint8_t *data);
   void handleCANMsgIntState(uint8_t *data);
   void handleCANMsgFaults(uint8_t *data);
   void handleCANMsgTorqueTimer(uint8_t *data);
   void handleCANMsgModFluxWeaken(uint8_t *data);
   void handleCANMsgFirmwareInfo(uint8_t *data);
   void handleCANMsgDiagnostic(uint8_t *data);
};

#endif /* RMSCTRL_H_ */



