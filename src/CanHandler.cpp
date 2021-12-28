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

#include "CanHandler.h"
#include "sys_io.h"
#include "devices/misc/SystemDevice.h"

/*
CAN1 is regular can but with some chip changes could be put into SWCAN mode.
CAN2 is isolated
CAN3 is CAN-FD capable. GEVCU7A boards failed to get an FD transceiver though.

Not using hardware filtering right now. CAN buses aren't really that fast and this is a
very fast chip. But, still it might not be a bad idea to eventually try it.
FIFOs are not available for CAN-FD mode but CAN-FD mode is not being attempted right now
*/

CanHandler canHandlerBus0 = CanHandler(CanHandler::CAN_BUS_0);
CanHandler canHandlerBus1 = CanHandler(CanHandler::CAN_BUS_1);
CanHandler canHandlerBus2 = CanHandler(CanHandler::CAN_BUS_2);

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0; //Reg CAN or SWCAN depending on mode
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can1; //Isolated CAN
FlexCAN_T4FD<CAN3, RX_SIZE_256, TX_SIZE_16> Can2; //Only CAN-FD capable output

void canRX0(const CAN_message_t &msg) 
{
    canHandlerBus0.process(msg);
}

void canRX1(const CAN_message_t &msg) 
{
    canHandlerBus1.process(msg); 
}

void canRX2(const CANFD_message_t &msg) 
{
    canHandlerBus2.process(msg);
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
    swmode = SW_SLEEP;
}

/*
 * Initialization of the CAN bus
 */
void CanHandler::setup()
{
    // Initialize the canbus at the specified baudrate
    uint16_t storedVal;
    uint32_t realSpeed;
    uint32_t fdSpeed;
    CANFD_timings_t fdTimings;
    int busNum = 0;

    //these pins control whether differential CAN or SingleWire CAN is found on CAN0
    pinMode(33, OUTPUT);
    pinMode(MODE0_PIN, OUTPUT);
    pinMode(MODE1_PIN, OUTPUT);

    switch (canBusNode)
    {
    case CAN_BUS_0:
        realSpeed = sysConfig->canSpeed[0];
        if (realSpeed < 33333ul) realSpeed = 33333u; 
        if (realSpeed > 1000000ul) realSpeed = 1000000ul;
        busSpeed = realSpeed;
        if (busSpeed > 0)
        {
            busNum = 0;                    
            Can0.begin();
            Can0.setBaudRate(realSpeed);
            Can0.setMaxMB(16);
            //Can0.enableFIFO();
            //Can0.enableFIFOInterrupt();
            Can0.enableMBInterrupts();
            Can0.onReceive(canRX0);
            setSWMode(SW_SLEEP);
        }
        else Can0.reset();
        break;
    case CAN_BUS_1:
        realSpeed = sysConfig->canSpeed[1];
        if (realSpeed < 33333ul) realSpeed = 33333u; 
        if (realSpeed > 1000000ul) realSpeed = 1000000ul;
        busSpeed = realSpeed;
        if (busSpeed > 0)
        {
            busNum = 1;            
            Can1.begin();
            Can1.setBaudRate(realSpeed);
            Can1.setMaxMB(16);
            //Can1.enableFIFO();
            //Can1.enableFIFOInterrupt();
            Can1.enableMBInterrupts();
            Can1.onReceive(canRX1);
        }
        else Can1.reset();
        break;
    case CAN_BUS_2:
        realSpeed = sysConfig->canSpeed[2];
        if (realSpeed < 33333ul) realSpeed = 33333u; 
        if (realSpeed > 1000000ul) realSpeed = 1000000ul;
        busSpeed = realSpeed;
        fdSpeed = sysConfig->canSpeed[3];
        if (fdSpeed < 500000ul) fdSpeed = 500000u; 
        if (fdSpeed > 8000000ul) fdSpeed = 8000000ul;
        if (busSpeed > 0)
        {
            busNum = 2;
            Can2.begin();
            fdTimings.baudrate = realSpeed;
            fdTimings.baudrateFD = fdSpeed;
            if (fdSpeed < 2000000ul) fdTimings.clock = 24;
            else fdTimings.clock = 40;
            Can2.setBaudRate(fdTimings);
            //Can2.setMaxMB(16);
            //Can2.enableFIFO();
            //Can2.enableFIFOInterrupt();
            Can2.enableMBInterrupts();
            Can2.onReceive(canRX2);
        }
        //else Can2.reset();
        break;
    }

    Logger::info("CAN%d init ok. Speed = %i", busNum, busSpeed);
}

void CanHandler::setSWMode(SWMode newMode)
{
    if (canBusNode != CAN_BUS_0) return; //naughty!
    swmode = newMode;
    switch (swmode)
    {
    case SW_SLEEP:
        digitalWrite(MODE0_PIN, LOW); //turn off SWCAN transceiver
        digitalWrite(MODE1_PIN, LOW);
        digitalWrite(33, HIGH); //turn on CAN1 transceiver    
        break;
    case SW_HVWAKE:
        digitalWrite(MODE0_PIN, LOW);
        digitalWrite(MODE1_PIN, HIGH);
        digitalWrite(33, LOW); //turn off CAN1 transceiver
        break;
    case SW_HISPEED:
        digitalWrite(MODE0_PIN, HIGH);
        digitalWrite(MODE1_PIN, LOW);
        digitalWrite(33, LOW); //turn off CAN1 transceiver
        break;
    case SW_NORMAL:
        digitalWrite(MODE0_PIN, HIGH);
        digitalWrite(MODE1_PIN, HIGH);
        digitalWrite(33, LOW); //turn off CAN1 transceiver
        break;
    }
}

SWMode CanHandler::getSWMode()
{
    return swmode;
}


uint32_t CanHandler::getBusSpeed()
{
    return busSpeed;
}

uint32_t CanHandler::getBusFDSpeed()
{
    return fdSpeed;
}

void CanHandler::setBusSpeed(uint32_t newSpeed)
{
    CANFD_timings_t fdTimings;
    uint32_t fdSpeed;
    int busNum = 0;
    if (canBusNode == CAN_BUS_2) 
    {
        busNum = 2;
        busSpeed = newSpeed;
        if (busSpeed > 0)
        {
            fdTimings.baudrate = newSpeed;
            fdSpeed = (newSpeed >= 500000ul) ? newSpeed : 500000ul;
            fdTimings.baudrateFD = fdSpeed;
            if (fdSpeed < 2000000ul) fdTimings.clock = 24;
            else fdTimings.clock = 40;
            Can2.setBaudRate(fdTimings);
        }
        //else Can2.reset();
    }
    else if (canBusNode == CAN_BUS_0)
    {
        busNum = 0;
        busSpeed = newSpeed;
        if (busSpeed > 0)
        {
            Can0.setBaudRate(busSpeed);
        }
        else Can0.reset();
    }
    else if (canBusNode == CAN_BUS_1)
    {
        busNum = 1;
        busSpeed = newSpeed;
        if (busSpeed > 0)
        {
            Can1.begin();
            Can1.setBaudRate(busSpeed);
            Can1.setMaxMB(16);
            Can1.enableMBInterrupts();
            Can1.onReceive(canRX1);
        }
    }

    Logger::info("CAN%d init ok. Speed = %i", busNum, busSpeed);
}

void CanHandler::setBusFDSpeed(uint32_t nomSpeed, uint32_t dataSpeed)
{
    CANFD_timings_t fdTimings;
    if (canBusNode != CAN_BUS_2) return; //only CAN2 can do FD mode
    if (nomSpeed > 125000ul)
    {
        fdTimings.baudrate = nomSpeed;
        fdTimings.baudrateFD = dataSpeed;
        if (dataSpeed < 2000000ul) fdTimings.clock = 24;
        else fdTimings.clock = 40;
        Can2.setBaudRate(fdTimings);        
    }
    //else Can2.reset();
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
        Logger::debug("CAN: bus=%i id=%X dlc=%X ide=%X data=%X,%X,%X,%X,%X,%X,%X,%X",
                      (int)canBusNode, msg.id, msg.len, msg.flags.extended,
                      msg.buf[0], msg.buf[1], msg.buf[2], msg.buf[3],
                      msg.buf[4], msg.buf[5], msg.buf[6], msg.buf[7]);
    }
}

//obviously it should be actually using FD not just normal CAN.... TODO
void CanHandler::logFrame(const CANFD_message_t &msg_fd)
{
    if (Logger::isDebug()) {
        Logger::debug("CANFD: bus=%i id=%X dlc=%X ide=%X data=%X,%X,%X,%X,%X,%X,%X,%X",
                      (int)canBusNode, msg_fd.id, msg_fd.len, msg_fd.flags.extended,
                      msg_fd.buf[0], msg_fd.buf[1], msg_fd.buf[2], msg_fd.buf[3],
                      msg_fd.buf[4], msg_fd.buf[5], msg_fd.buf[6], msg_fd.buf[7]);
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

void CanHandler::process(const CANFD_message_t &msgfd)
{
    static SDO_FRAME sFrame;

    CanObserver *observer;

    //see if we can turn this into a standard CAN frame and process it via that interface. Otherwise
    //continue with CAN-FD interpretation
    if ( (msgfd.brs == 0) && (msgfd.edl == 0) && (msgfd.len < 9) )
    {
        CAN_message_t msg;
        msg.id = msgfd.id;
        msg.bus = msgfd.bus;
        msg.len = msgfd.len;
        msg.timestamp = msgfd.timestamp;
        msg.flags.extended = msgfd.flags.extended;
        for (int i = 0; i < msg.len; i++) msg.buf[i] = msgfd.buf[i];
        process(msg);
        return;
    }    

    for (int i = 0; i < CFG_CAN_NUM_OBSERVERS; i++) 
    {
        observer = observerData[i].observer;
        if (observer != NULL) {
            if ((msgfd.id & observerData[i].mask) == (observerData[i].id & observerData[i].mask)) {
                observer->handleCanFDFrame(msgfd);
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
    case CAN_BUS_0:
        Can0.write(msg);
        break;
    case CAN_BUS_1:    
        Can1.write(msg);
        break;
    case CAN_BUS_2:
        //can't do this directly. Have to package it into a CANFD frame to send
        CANFD_message_t fdMsg;
        fdMsg.id = msg.id;
        fdMsg.brs = 0; //no rate switching
        fdMsg.edl = 0; //no extended data length either
        fdMsg.len = msg.len;
        fdMsg.flags.extended = msg.flags.extended;
        for (int i = 0; i < msg.len; i++) fdMsg.buf[i] = msg.buf[i];
        Can2.write(fdMsg);
        break;            
    }
}

void CanHandler::sendFrameFD(const CANFD_message_t& framefd)
{
    if (canBusNode != CAN_BUS_2) return;
    Can2.write(framefd);
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

void CanObserver::setNodeID(unsigned int id)
{
    nodeID = id & 0x7F;
}

unsigned int CanObserver::getNodeID()
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
    Logger::error("CanObserver does not implement handleCanFrame(), frame.id=0x%x", frame.id);
}

void CanObserver::handleCanFDFrame(const CANFD_message_t &frame_fd)
{
    //we will handle the case where this was called but really the traffic was standard CAN.
    //In that case, try to massage the data and send it as normal CAN
    if ( (frame_fd.brs == 0) && (frame_fd.edl == 0) && (frame_fd.len < 9) )
    {
        CAN_message_t msg;
        msg.id = frame_fd.id;
        msg.bus = frame_fd.bus;
        msg.len = frame_fd.len;
        msg.timestamp = frame_fd.timestamp;
        msg.flags.extended = frame_fd.flags.extended;
        for (int i = 0; i < msg.len; i++) msg.buf[i] = frame_fd.buf[i];
        handleCanFrame(msg);
    }
    else Logger::error("CanObserver does not implement handleCanFDFrame(), frame.id=0x%x", frame_fd.id);
}

void CanObserver::handlePDOFrame(const CAN_message_t &frame)
{
    Logger::error("CanObserver does not implement handlePDOFrame(), frame.id=0x%x", frame.id);
}

void CanObserver::handleSDORequest(SDO_FRAME &frame)
{
    Logger::error("CanObserver does not implement handleSDORequest(), frame.id=0x%x", frame.nodeID);
}

void CanObserver::handleSDOResponse(SDO_FRAME &frame)
{
    Logger::error("CanObserver does not implement handleSDOResponse(), frame.id=%d", frame.nodeID);
}
