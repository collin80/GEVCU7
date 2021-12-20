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

class CanObserver
{
public:
    CanObserver();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void handlePDOFrame(const CAN_message_t &frame);
    virtual void handleSDORequest(SDO_FRAME &frame);
    virtual void handleSDOResponse(SDO_FRAME &frame);
    void setCANOpenMode(bool en);
    bool isCANOpen();
    void setNodeID(unsigned int id);
    unsigned int getNodeID();

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
    uint32_t getBusSpeed();
    void setBusSpeed(uint32_t newSpeed);
    void attach(CanObserver *observer, uint32_t id, uint32_t mask, bool extended);
    void detach(CanObserver *observer, uint32_t id, uint32_t mask);
    void process(const CAN_message_t &msg);
    void prepareOutputFrame(CAN_message_t &frame, uint32_t id);
    void CANIO(const CAN_message_t& frame);
    void sendFrame(const CAN_message_t& frame);
    void sendISOTP(int id, int length, uint8_t *data);
    void setSWMode(SWMode newMode);
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
    SWMode swmode;

    void logFrame(const CAN_message_t &msg);
    int8_t findFreeObserverData();
    int8_t findFreeMailbox();

    //canopen support functions
    void sendNMTMsg(int, int);
    int masterID; //what is our ID as the master node?      
};

extern CanHandler canHandlerBus0;
extern CanHandler canHandlerBus1;
extern CanHandler canHandlerBus2;
extern CanHandler canHandlerSingleWire;

#endif /* CAN_HANDLER_H_ */