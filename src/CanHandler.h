/*
 * CanHandler.h

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

#ifndef CAN_HANDLER_H_
#define CAN_HANDLER_H_

#include <Arduino.h>
#include "config.h"
#include <FlexCAN_T4.h>
#include "Logger.h"

//CAN message ID ASSIGNMENTS FOR I/0 MANAGEMENT
//should make these configurable.
#define CAN_SWITCH 0x606
#define CAN_OUTPUTS 0x607
#define CAN_ANALOG_INPUTS 0x608
#define CAN_DIGITAL_INPUTS 0x609

#define MODE0_PIN   26
#define MODE1_PIN   32

enum SDO_COMMAND
{
    SDO_WRITE = 0x20,
    SDO_READ = 0x40,
    SDO_WRITEACK = 0x60,
};

struct SDO_FRAME
{
    uint8_t nodeID;
    SDO_COMMAND cmd;
    uint16_t index;
    uint8_t subIndex;
    uint8_t dataLength;
    uint8_t data[4];
};

enum ISOTP_MODE
{
    SINGLE = 0,
    FIRST = 1,
    CONSEC = 2,
    FLOW = 3
};

class CanHandler;

class CanObserver
{
public:
    CanObserver();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void handleCanFDFrame(const CANFD_message_t &framefd);
    virtual void handlePDOFrame(const CAN_message_t &frame);
    virtual void handleSDORequest(SDO_FRAME &frame);
    virtual void handleSDOResponse(SDO_FRAME &frame);
    void setCANOpenMode(bool en);
    bool isCANOpen();
    void setNodeID(unsigned int id);
    unsigned int getNodeID();

protected:
    CanHandler *attachedCANBus;
    void setAttachedCANBus(int bus);

private:
    bool canOpenMode;
    unsigned int nodeID;
};

enum SWMode
{
    SW_SLEEP,
    SW_HVWAKE,
    SW_HISPEED,
    SW_NORMAL
};

enum GVRET_STATE {
    IDLE,
    GET_COMMAND,
    BUILD_CAN_FRAME,
    TIME_SYNC,
    GET_DIG_INPUTS,
    GET_ANALOG_INPUTS,
    SET_DIG_OUTPUTS,
    SETUP_CANBUS,
    GET_CANBUS_PARAMS,
    GET_DEVICE_INFO,
    SET_SINGLEWIRE_MODE,
    SET_SYSTYPE,
    ECHO_CAN_FRAME,
    SETUP_EXT_BUSES,
    BUILD_FD_FRAME
};

enum GVRET_PROTOCOL
{
    PROTO_BUILD_CAN_FRAME = 0,
    PROTO_TIME_SYNC = 1,
    PROTO_DIG_INPUTS = 2,
    PROTO_ANA_INPUTS = 3,
    PROTO_SET_DIG_OUT = 4,
    PROTO_SETUP_CANBUS = 5,
    PROTO_GET_CANBUS_PARAMS = 6,
    PROTO_GET_DEV_INFO = 7,
    PROTO_SET_SW_MODE = 8,
    PROTO_KEEPALIVE = 9,
    PROTO_SET_SYSTYPE = 10,
    PROTO_ECHO_CAN_FRAME = 11,
    PROTO_GET_NUMBUSES = 12,
    PROTO_GET_EXT_BUSES = 13,
    PROTO_SET_EXT_BUSES = 14,
    PROTO_BUILD_FD_FRAME = 20,
    PROTO_SETUP_FD = 21,
    PROTO_GET_FD = 22,
};

class CanHandler
{
public:
    enum CanBusNode {
        CAN_BUS_0,
        CAN_BUS_1,
        CAN_BUS_2
    };

    CanHandler(CanBusNode busNumber);
    void setup();
    void loop();
    uint32_t getBusSpeed();
    uint32_t getBusFDSpeed();
    void setBusSpeed(uint32_t newSpeed);
    void setBusFDSpeed(uint32_t nomSpeed, uint32_t dataSpeed);
    void attach(CanObserver *observer, uint32_t id, uint32_t mask, bool extended);
    void detach(CanObserver *observer, uint32_t id, uint32_t mask);
    void detachAll(CanObserver *observer);
    void process(const CAN_message_t &msg);
    void process(const CANFD_message_t &msg_fd);
    void prepareOutputFrame(CAN_message_t &frame, uint32_t id);
    void CANIO(const CAN_message_t& frame);
    void sendFrame(const CAN_message_t& frame);
    void sendFrameFD(const CANFD_message_t& framefd);
    void sendISOTP(int id, int length, uint8_t *data);
    void setSWMode(SWMode newMode);
    void setGVRETMode(bool mode);
    SWMode getSWMode();

    //canopen support functions
    void sendNodeStart(int id = 0);
    void sendNodePreop(int id = 0);
    void sendNodeReset(int id = 0);
    void sendNodeStop(int id = 0);
    void sendPDOMessage(int, int, unsigned char *);
    void sendSDORequest(SDO_FRAME &frame);
    void sendSDOResponse(SDO_FRAME &frame);
    void sendHeartbeat();
    void setMasterID(int id);

protected:

private:
    struct CanObserverData {
        uint32_t id;    // what id to listen to
        uint32_t mask;  // the CAN frame mask to listen to
        bool extended;  // are extended frames expected
        uint8_t mailbox;    // which mailbox is this observer assigned to
        CanObserver *observer;  // the observer object (e.g. a device)
    };

    CanBusNode canBusNode;  // indicator to which can bus this instance is assigned to
    CanObserverData observerData[CFG_CAN_NUM_OBSERVERS];    // Can observers
    uint32_t busSpeed;
    uint32_t fdSpeed;
    SWMode swmode;
    bool binOutput;
    GVRET_STATE gvretState;
    int gvretStep;
    bool gvretMode;
    CAN_message_t build_out_frame;
    CANFD_message_t build_out_fd;

    void logFrame(const CAN_message_t &msg);
    void logFrame(const CANFD_message_t &msg_fd);
    int8_t findFreeObserverData();
    int8_t findFreeMailbox();
    uint8_t checksumCalc(uint8_t *buffer, int length);
    void sendFrameToUSB(const CAN_message_t &msg, int busNum = -1);
    void sendFrameToUSB(const CANFD_message_t &msg, int busNum = -1);

    //canopen support functions
    void sendNMTMsg(int, int);
    int masterID; //what is our ID as the master node?      
};

void canEvents();

extern CanHandler canHandlerBus0;
extern CanHandler canHandlerBus1;
extern CanHandler canHandlerBus2;

//handy names for the special buses so it is explicit why they're being used.
#define canHandlerIsolated canHandlerBus1
#define canHandlerFD canHandlerBus2

#endif /* CAN_HANDLER_H_ */