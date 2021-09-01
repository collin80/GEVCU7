/*
 * CanHandler.cpp
 *
 * Devices may register to this handler in order to receive CAN frames (publish/subscribe)
 * and they can also use this class to send messages.
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

/* 
Really, I think the outside API doesn't need to change necessarily. Most
of the things that aren't the same with this new CAN library are different
in this handler code. But, I'm contemplating switching over to natively
use the FlexCAN API. This would make devices not even remotely compatible
with GEVCU6 and GEVCU4/5 though. Not sure I want to do that.
*/

#include "CanHandler.h"
#include "sys_io.h"

CanHandler canHandlerEv = CanHandler(CanHandler::CAN_BUS_EV);
CanHandler canHandlerCar = CanHandler(CanHandler::CAN_BUS_CAR);
CanHandler canHandlerCar2 = CanHandler(CanHandler::CAN_BUS_CAR2);
CanHandler canHandlerSingleWire = CanHandler(CanHandler::CAN_BUS_SW);

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can1;
FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> Can2;

void canRX0(const CAN_message_t &msg) 
{
    canHandlerEv.process(msg);
}

void canRX1(const CAN_message_t &msg) 
{
    canHandlerCar.process(msg); 
}

void canRX2(const CAN_message_t &msg) 
{
    canHandlerCar2.process(msg);
}

void canRX3(const CAN_message_t &msg) 
{
    canHandlerSingleWire.process(msg);
}

/*
 * Constructor of the can handler
 */
CanHandler::CanHandler(CanBusNode canBusNode)
{
    this->canBusNode = canBusNode;

    for (int i = 0; i < CFG_CAN_NUM_OBSERVERS; i++) {
        observerData[i].observer = NULL;
    }
    masterID = 0x05;
    busSpeed = 0;
}

/*
 * Initialization of the CAN bus
 */
void CanHandler::setup()
{
    // Initialize the canbus at the specified baudrate
    uint16_t storedVal;
    uint32_t realSpeed;
    int busNum = 0;

    pinMode(33, OUTPUT);
    pinMode(26, OUTPUT);
    pinMode(32, OUTPUT);

    if (canBusNode == CAN_BUS_EV) 
    {
        sysPrefs->read(EESYS_CAN0_BAUD, &storedVal);
        busNum = 0;
        Can0.begin();
        Can0.setBaudRate(realSpeed);
        Can0.setMaxMB(16);
        //Can0.enableFIFO();
        //Can0.enableFIFOInterrupt();
        Can0.enableMBInterrupts();
        Can0.onReceive(canRX0);
    }
    else if (canBusNode == CAN_BUS_CAR)
    {
        sysPrefs->read(EESYS_CAN1_BAUD, &storedVal);
        busNum = 1;
        Can1.begin();
        Can1.setBaudRate(realSpeed);
        Can1.setMaxMB(16);
        //Can1.enableFIFO();
        //Can1.enableFIFOInterrupt();
        Can1.enableMBInterrupts();
        Can1.onReceive(canRX0);        
    }
    else if (canBusNode == CAN_BUS_CAR2)
    {
        digitalWrite(26, LOW); //turn off SWCAN transceiver
        digitalWrite(32, LOW);
        digitalWrite(33, HIGH); //turn on CAN1 transceiver
        sysPrefs->read(EESYS_CAN2_BAUD, &storedVal);
        busNum = 2;
        Can2.begin();
        Can2.setBaudRate(realSpeed);
        Can2.setMaxMB(16);
        //Can2.enableFIFO();
        //Can2.enableFIFOInterrupt();
        Can2.enableMBInterrupts();
        Can2.onReceive(canRX0);            
    }
    else //swcan
    {
        digitalWrite(33, LOW); //turn off CAN1 transceiver
        digitalWrite(26, HIGH); //turn on SWCAN transceiver to normal mode
        digitalWrite(32, HIGH);
        sysPrefs->read(EESYS_SWCAN_BAUD, &storedVal);
        busNum = 3;
        Can1.begin();
        Can1.setBaudRate(realSpeed);
        Can1.setMaxMB(16);
        //Can1.enableFIFO();
        //Can1.enableFIFOInterrupt();
        Can1.enableMBInterrupts();
        Can1.onReceive(canRX0);    
    }

    realSpeed = storedVal * 1000; //was stored in thousands, now in actual rate
    if (realSpeed < 33333ul) realSpeed = 33333u; 
    if (realSpeed > 1000000ul) realSpeed = 1000000ul;

    busSpeed = realSpeed;
 
    Logger::info("CAN%d init ok. Speed = %i", busNum, busSpeed);
}

uint32_t CanHandler::getBusSpeed()
{
    return busSpeed;
}

/*
 * Attach a CanObserver. Can frames which match the id/mask will be forwarded to the observer
 * via the method handleCanFrame(RX_CAN_FRAME).
 * Sets up a can bus mailbox if necessary.
 *
 *  \param observer - the observer object to register (must implement CanObserver class)
 *  \param id - the id of the can frame to listen to
 *  \param mask - the mask to be applied to the frames
 *  \param extended - set if extended frames must be supported
 */
void CanHandler::attach(CanObserver* observer, uint32_t id, uint32_t mask, bool extended)
{
    int8_t pos = findFreeObserverData();

    if (pos == -1) {
        Logger::error("no free space in CanHandler::observerData, increase its size via CFG_CAN_NUM_OBSERVERS");
        return;
    }

    //int mailbox = bus->findFreeRXMailbox();

    //if (mailbox == -1) {
    //    Logger::error("no free CAN mailbox on bus %d", canBusNode);
    //    return;
    //}

    observerData[pos].id = id;
    observerData[pos].mask = mask;
    observerData[pos].extended = extended;
    observerData[pos].mailbox = 0;//mailbox;
    observerData[pos].observer = observer;

    //bus->setMBUserFilter(mailbox, id, mask);

    //Logger::debug("attached CanObserver (%X) for id=%X, mask=%X, mailbox=%d", observer, id, mask, mailbox);
    Logger::debug("attached CanObserver (%X) for id=%X, mask=%X", observer, id, mask);
}

/*
 * Detaches a previously attached observer from this handler.
 *
 * \param observer - observer object to detach
 * \param id - id of the observer to detach (required as one CanObserver may register itself several times)
 * \param mask - mask of the observer to detach (dito)
 */
void CanHandler::detach(CanObserver* observer, uint32_t id, uint32_t mask)
{
    for (int i = 0; i < CFG_CAN_NUM_OBSERVERS; i++) {
        if (observerData[i].observer == observer &&
                observerData[i].id == id &&
                observerData[i].mask == mask) {
            observerData[i].observer = NULL;

            //TODO: if no more observers on same mailbox, disable its interrupt, reset mailbox
        }
    }
}

/*
 * Logs the content of a received can frame
 *
 * \param frame - the received can frame to log
 */
void CanHandler::logFrame(const CAN_message_t &msg)
{
    
    if (Logger::isDebug()) {
        Logger::debug("CAN: id=%X dlc=%X ide=%X data=%X,%X,%X,%X,%X,%X,%X,%X",
                      msg.id, msg.len, msg.flags.extended,
                      msg.buf[0], msg.buf[1], msg.buf[2], msg.buf[3],
                      msg.buf[4], msg.buf[5], msg.buf[6], msg.buf[7]);
    }
}

/*
 * Find a observerData entry which is not in use.
 *
 * \retval array index of the next unused entry in observerData[]
 */
int8_t CanHandler::findFreeObserverData()
{
    for (int i = 0; i < CFG_CAN_NUM_OBSERVERS; i++) {
        if (observerData[i].observer == NULL) {
            return i;
        }
    }

    return -1;
}

/*
 * Find a unused can mailbox according to entries in observerData[].
 *
 * \retval the mailbox index of the next unused mailbox
 */
int8_t CanHandler::findFreeMailbox()
{
/*    uint8_t numRxMailboxes = 6; //there are 8 total and two are used for TX

    for (uint8_t i = 0; i < numRxMailboxes; i++) {
        bool used = false;

        for (uint8_t j = 0; j < CFG_CAN_NUM_OBSERVERS; j++) {
            if (observerData[j].observer != NULL && observerData[j].mailbox == i) {
                used = true;
            }
        }

        if (!used) {
            return i;
        }
    }
*/
    return -1;
}

/*
 * If a message is available, read it and forward it to registered observers.
 */
void CanHandler::process(const CAN_message_t &msg)
{
    static SDO_FRAME sFrame;

    CanObserver *observer;

    if(msg.id == CAN_SWITCH) CANIO(msg);
    for (int i = 0; i < CFG_CAN_NUM_OBSERVERS; i++) 
    {
        observer = observerData[i].observer;
        if (observer != NULL) {
            // Apply mask to frame.id and observer.id. If they match, forward the frame to the observer
            if (observer->isCANOpen())
            {
                if (msg.id > 0x17F && msg.id < 0x580)
                {
                    observer->handlePDOFrame(msg);
                }
                if (msg.id == 0x600 + observer->getNodeID()) //SDO request targetted to our ID
                {
                    sFrame.nodeID = observer->getNodeID();
                    sFrame.index = msg.buf[1] + (msg.buf[2] * 256);
                    sFrame.subIndex = msg.buf[3];
                    sFrame.cmd = (SDO_COMMAND)(msg.buf[0] & 0xF0);
            
                    if ((msg.buf[0] != 0x40) && (msg.buf[0] != 0x60))
                    {
                        sFrame.dataLength = (3 - ((msg.buf[0] & 0xC) >> 2)) + 1;            
                    }
                    else sFrame.dataLength = 0;

                    for (int x = 0; x < sFrame.dataLength; x++) sFrame.data[x] = msg.buf[4 + x];
                    observer->handleSDORequest(sFrame);
                }

                if (msg.id == 0x580 + observer->getNodeID()) //SDO reply to our ID
                {
                    sFrame.nodeID = observer->getNodeID();
                    sFrame.index = msg.buf[1] + (msg.buf[2] * 256);
                    sFrame.subIndex = msg.buf[3];
                    sFrame.cmd = (SDO_COMMAND)(msg.buf[0] & 0xF0);
            
                    if ((msg.buf[0] != 0x40) && (msg.buf[0] != 0x60))
                    {
                        sFrame.dataLength = (3 - ((msg.buf[0] & 0xC) >> 2)) + 1;            
                    }
                    else sFrame.dataLength = 0;

                    for (int x = 0; x < sFrame.dataLength; x++) sFrame.data[x] = msg.buf[4 + x];

                    observer->handleSDOResponse(sFrame);                       
                }
            }
            else //raw canbus
            {
                if ((msg.id & observerData[i].mask) == (observerData[i].id & observerData[i].mask)) {
                    observer->handleCanFrame(msg);
                }
            }
        }
    }
}

/*
 * Prepare the CAN transmit frame.
 * Re-sets all parameters in the re-used frame.
 */
void CanHandler::prepareOutputFrame(CAN_message_t &msg, uint32_t id)
{
    msg.len = 8;
    msg.id = id;
    msg.flags.extended = 0;

    msg.buf[0] = 0;
    msg.buf[1] = 0;
    msg.buf[2] = 0;
    msg.buf[3] = 0;
    msg.buf[4] = 0;
    msg.buf[5] = 0;
    msg.buf[6] = 0;
    msg.buf[7] = 0;
}

void CanHandler::CANIO(const CAN_message_t &msg) {
    static CAN_message_t CANioFrame;
    int i;

  Logger::warn("CANIO %d msg: %X   %X   %X   %X   %X   %X   %X   %X  %X", canBusNode, msg.id, msg.buf[0],
                  msg.buf[1], msg.buf[2], msg.buf[3], msg.buf[4],
                  msg.buf[5], msg.buf[6], msg.buf[7]);

    CANioFrame.id = CAN_OUTPUTS;
    CANioFrame.len = 8;
    CANioFrame.flags.extended = 0; //standard frame  
  
    //handle the incoming frame to set/unset/leave alone each digital output
    for(i = 0; i < 8; i++) {
        if (msg.buf[i] == 0x88) systemIO.setDigitalOutput(i,true);
        if (msg.buf[i] == 0xFF) systemIO.setDigitalOutput(i,false);
    }
  
    for(i = 0; i < 8; i++) {
        if (systemIO.getDigitalOutput(i)) CANioFrame.buf[i] = 0x88;
        else CANioFrame.buf[i] = 0xFF;
    }
     
    sendFrame(CANioFrame);
        
    CANioFrame.id = CAN_ANALOG_INPUTS;
    i = 0;
    int16_t anaVal;
       
    for(int j = 0; j < 8; j += 2) {
        anaVal = systemIO.getAnalogIn(i++);
        CANioFrame.buf[j] = highByte (anaVal);
        CANioFrame.buf[j + 1] = lowByte(anaVal);
    }
        
    sendFrame(CANioFrame);

    CANioFrame.id = CAN_DIGITAL_INPUTS;
    CANioFrame.len = 4;
  
    for(i = 0; i < 4; i++) {
        if (systemIO.getDigitalIn(i)) CANioFrame.buf[i] = 0x88;
        else CANioFrame.buf[i] = 0xff;
    }
      
    sendFrame(CANioFrame);
}


//Allow the canbus driver to figure out the proper mailbox to use
//(whatever happens to be open) or queue it to send (if nothing is open)
void CanHandler::sendFrame(const CAN_message_t &msg)
{
    switch (canBusNode)
    {
    case CAN_BUS_CAR:
        Can2.write(msg);
        break;
    case CAN_BUS_EV:
        Can0.write(msg);
        break;
    case CAN_BUS_CAR2:
    case CAN_BUS_SW:
        Can1.write(msg);
        break;    
    }
}

void CanHandler::sendISOTP(int id, int length, uint8_t *data)
{
    CAN_message_t frame;

    frame.flags.extended = false;
    frame.id = id;

    if (length < 8) //single frame
    {
        frame.len = length + 1;
        frame.buf[0] = SINGLE + (length << 4);
        for (int i = 0; i < length; i++) frame.buf[i + 1] = data[i];
        sendFrame(frame);
    }
    else //multi-frame sending
    {
        int temp = length;
        uint8_t idx = 0;
        int base;
        frame.len = 8;
        frame.buf[0] = FIRST + (length >> 8);
        frame.buf[1] = (length & 0xFF);
        for (int i = 0; i < 6; i++) frame.buf[i + 2] = data[i];
        sendFrame(frame);
        temp -= 6;
        base = 6;
        while (temp > 7)
        {
            frame.len = 8;
            frame.buf[0] = CONSEC + (idx << 4);
            idx = (idx + 1) & 0xF;
            for (int i = 0; i < 7; i++) frame.buf[i + 1] = data[i + base];
            sendFrame(frame);
            temp -= 7;
            base += 7;
        }
        if (temp > 0)
        {
            frame.len = temp + 1;
            frame.buf[0] = CONSEC + (idx << 4);
            for (int i = 0; i < temp; i++) frame.buf[i + 1] = data[i + base];
            sendFrame(frame);
        }
    }
}

void CanHandler::sendNodeStart(int id)
{
    sendNMTMsg(id, 1);
}

void CanHandler::sendNodePreop(int id)
{
    sendNMTMsg(id, 0x80);
}

void CanHandler::sendNodeReset(int id)
{
    sendNMTMsg(id, 0x81);
}

void CanHandler::sendNodeStop(int id)
{
    sendNMTMsg(id, 2);
}

void CanHandler::sendPDOMessage(int id, int length, unsigned char *data)
{
    if (id > 0x57F) return; //invalid ID for a PDO message
    if (id < 0x180) return; //invalid ID for a PDO message
    if (length > 8 || length < 0) return; //invalid length
    CAN_message_t frame;
    frame.id = id;
    frame.flags.extended = false;
    frame.len = length;
    for (int x = 0; x < length; x++) frame.buf[x] = data[x];
    sendFrame(frame);
}

void CanHandler::sendSDORequest(SDO_FRAME &sframe)
{
    sframe.nodeID &= 0x7F;
    CAN_message_t frame;
    frame.flags.extended = false;
    frame.len = 8;
    frame.id = 0x600 + sframe.nodeID;
    if (sframe.dataLength <= 4)
    {
        frame.buf[0] = sframe.cmd;
        if (sframe.dataLength > 0) //request to write data
        {
            frame.buf[0] |= 0x0F - ((sframe.dataLength - 1) * 4); //kind of dumb the way this works...
        }
        frame.buf[1] = sframe.index & 0xFF;
        frame.buf[2] = sframe.index >> 8;
        frame.buf[3] = sframe.subIndex;
        for (int x = 0; x < sframe.dataLength; x++) frame.buf[4 + x] = sframe.data[x];
        //SerialUSB.println("pulling trigger");
        sendFrame(frame);
        //SerialUSB.println("sent frame");
    }
}

void CanHandler::sendSDOResponse(SDO_FRAME &sframe)
{
    sframe.nodeID &= 0x7f;
    CAN_message_t frame;
    frame.len = 8;
    frame.flags.extended = false;
    frame.id = 0x580 + sframe.nodeID;
    if (sframe.dataLength <= 4)
    {
        frame.buf[0] = sframe.cmd;
        if (sframe.dataLength > 0) //responding with data
        {
            frame.buf[0] |= 0x0F - ((sframe.dataLength - 1) * 4); 
        }
        frame.buf[1] = sframe.index & 0xFF;
        frame.buf[2] = sframe.index >> 8;
        frame.buf[3] = sframe.subIndex;
        for (int x = 0; x < sframe.dataLength; x++) frame.buf[4 + x] = sframe.data[x];
        sendFrame(frame);
    }
}

void CanHandler::sendHeartbeat()
{
    CAN_message_t frame;
    frame.id = 0x700 + masterID;
    frame.len = 1;
    frame.flags.extended = false;
    frame.buf[0] = 5; //we're always operational
    sendFrame(frame);  
}

void CanHandler::sendNMTMsg(int id, int cmd)
{       
    id &= 0x7F;
    CAN_message_t frame;
    frame.id = 0;
    frame.flags.extended = false;
    frame.len = 2;
    frame.buf[0] = cmd;
    frame.buf[1] = id;
    //the rest don't matter
    sendFrame(frame);
}

void CanHandler::setMasterID(int id)
{
    masterID = id;
}


CanObserver::CanObserver()
{
    canOpenMode = false;
    nodeID = 0x7F;
}

//setting can open mode causes the can handler to run its own handleCanFrame system where things are sorted out
void CanObserver::setCANOpenMode(bool en)
{
    canOpenMode = en;
}

void CanObserver::setNodeID(int id)
{
    nodeID = id & 0x7F;
}

int CanObserver::getNodeID()
{
    return nodeID;
}

bool CanObserver::isCANOpen()
{
    return canOpenMode;
}

/*
 * Default implementation of the CanObserver method. Must be overwritten
 * by every sub-class. However, canopen devices should still call this version to save themselves some effort
 */
void CanObserver::handleCanFrame(const CAN_message_t &frame)
{
    Logger::error("CanObserver does not implement handleCanFrame(), frame.id=%d", frame.id);
}

void CanObserver::handlePDOFrame(const CAN_message_t &frame)
{
    Logger::error("CanObserver does not implement handlePDOFrame(), frame.id=%d", frame.id);
}

void CanObserver::handleSDORequest(SDO_FRAME &frame)
{
    Logger::error("CanObserver does not implement handleSDORequest(), frame.id=%d", frame.nodeID);
}

void CanObserver::handleSDOResponse(SDO_FRAME &frame)
{
    Logger::error("CanObserver does not implement handleSDOResponse(), frame.id=%d", frame.nodeID);
}
