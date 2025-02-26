/*
 * UDSController.h
 *
Copyright (c) 2013-2021 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#ifndef UDS_CTRL_H_
#define UDS_CTRL_H_

#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <isotp.h>
#include "../../config.h"
#include "../io/Throttle.h"
#include "../../DeviceManager.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"
#include "../../constants.h"

#define UDSCONTROLLER 0x6000

enum UDS_CODE 
{
    OBDII_SHOW_CURRENT = 1,
    OBDII_SHOW_FREEZE = 2,
    OBDII_SHOW_STORED_DTC = 3,
    OBDII_CLEAR_DTC = 4,
    OBDII_TEST_O2 = 5,
    OBDII_TEST_RESULTS = 6,
    OBDII_SHOW_PENDING_DTC = 7,
    OBDII_CONTROL_DEVICES = 8,
    OBDII_VEH_INFO = 9,
    OBDII_PERM_DTC = 0xA,
    UDS_DIAG_CTRL = 0x10,
    UDS_ECU_RESET = 0x11,
    GMLAN_READ_FAILURE_RECORD = 0x12,
    UDS_CLEAR_DIAG = 0x14,
    UDS_READ_DTC = 0x19,
    GMLAN_READ_DIAG_ID = 0x1A,
    UDS_RETURN_NORM = 0x20,
    UDS_READ_BY_LOCAL_ID = 0x21,
    UDS_READ_BY_ID = 0x22,
    UDS_READ_BY_ADDR = 0x23,
    UDS_READ_SCALING_ID = 0x24,
    UDS_SECURITY_ACCESS = 0x27,
    UDS_COMM_CTRL = 0x28,
    UDS_READ_ID_PERIODIC = 0x2A,
    UDS_DYN_DATA_DEF = 0x2C,
    UDS_DEFINE_PID_BY_ADDR = 0x2D,
    UDS_WRITE_BY_ID = 0x2E,
    UDS_IO_CTRL = 0x2F,
    UDS_ROUTINE_CTRL = 0x31,
    UDS_REQUEST_DOWNLOAD = 0x34,
    UDS_REQUEST_UPLOAD = 0x35,
    UDS_TRANSFER_DATA = 0x36,
    UDS_REQUEST_TX_EXIT = 0x37,
    UDS_REQUEST_FILE_TX = 0x38,
    GMLAN_WRITE_DID = 0x3B,
    UDS_WRITE_BY_ADDR = 0x3D,
    UDS_TESTER_PRESENT = 0x3E,
    UDS_NEG_RESPONSE = 0x7F,
    UDS_ACCESS_TIMING = 0x83,
    UDS_SECURED_TX = 0x84,
    UDS_CTRL_DTC_SETTINGS = 0x85,
    UDS_RESPONSE_ON_EVENT = 0x86,
    UDS_RESPONSE_LINK_CTRL = 0x87,
    GMLAN_REPORT_PROG_STATE = 0xA2,
    GMLAN_ENTER_PROG_MODE = 0xA5,
    GMLAN_CHECK_CODES = 0xA9,
    GMLAN_READ_DPID = 0xAA,
    GMLAN_DEVICE_CTRL = 0xAE
};

//these two tables are randomly generated until I found values I liked.
//Feel free to change them but do note that it will invalidate any existing
//flashing programs.
uint8_t multTable[4] = {0x62, 0x84, 0x2B, 0xA3 };
uint8_t xorTable[4] = {0x91, 0xEB, 0x24, 0x5D};

class UDSConfiguration : public DeviceConfiguration {
public:
    uint32_t udsRx; //what ID are we listening for?
    uint32_t udsTx; //what ID do we send on?
    uint8_t useExtended;
    uint8_t udsBus; //which bus to listen on
    uint8_t listenBroadcast; //also listen on 0x7DF?
};

class UDSController: public Device, CanObserver {
public:
    UDSController();
    void setup();
    void handleTick();
    void handleCanFrame(const CAN_message_t &frame);
    void handleIsoTP(const ISOTP_data &iso_config, const uint8_t *buf);

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
    bool processShowData(const uint8_t *inData);
    bool processShowCustomData(const CAN_message_t &inFrame, CAN_message_t& outFrame);
    void generateChallenge();
    bool validateResponse(const uint8_t *bytes);
    uint8_t sendBuffer[512];
    uint8_t challenge[4];
    bool inSecurityMode;
    bool generatedSeed;
};

#endif


