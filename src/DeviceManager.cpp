/*
 * DeviceManager.cpp
 *
 The device manager keeps a list of all devices which are installed into the system.
 Anything that needs either a tick handler, a canbus handler, or to communicate to other
 devices on the system must be registered with the manager. The manager then handles
 making sure the tick handler is set up (if needed), the canbus handler is set up (if need),
 and that a device can send information to other devices by either type (BMS, motor ctrl, etc)
 or by device ID.

 The device class itself has the handlers defined so the tick and canbus handling code
 need only call these existing functions but the manager interface needs to
 expose a way to register them with the system.

 Copyright (c) 2021 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#include "DeviceManager.h"
#include "devices/display/StatusCSV.h"

const char *CFG_VAR_TYPE_NAMES[7] = {"BYTE","STRING","INT16","UINT16","INT32","UINT32","FLOAT"};

DeviceManager::DeviceManager() {
    throttle = nullptr;
    brake = nullptr;
    motorController = nullptr;
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++)
        devices[i] = nullptr;
}

/*
 * Add the specified device to the list of registered devices
 */
void DeviceManager::addDevice(Device *device) {
    if (findDevice(device) == -1) { //first check to make sure we haven't already added it.
        int8_t i = findDevice(NULL);
        if (i != -1) {
            devices[i] = device;
        } else {
            Logger::error("unable to register device, max number of devices reached.");
        }
    }
    /*
    switch (device->getType()) {
    case DEVICE_THROTTLE:
    	throttle = (Throttle *) device;
    	break;
    case DEVICE_BRAKE:
    	brake = (Throttle *) device;
    	break;
    case DEVICE_MOTORCTRL:
    	motorController = (MotorController *) device;
    	break;
    }
    */
}

/*
 * Remove the specified device from the list of registered devices
 */
void DeviceManager::removeDevice(Device *device) {
    int8_t i = findDevice(NULL);
    if (i != -1)
        devices[i] = NULL;
    switch (device->getType()) {
    case DEVICE_THROTTLE:
        throttle = NULL;
        break;
    case DEVICE_BRAKE:
        brake = NULL;
        break;
    case DEVICE_MOTORCTRL:
        motorController = NULL;
        break;
    case DEVICE_ANY:
    case DEVICE_BMS:
    case DEVICE_CHARGER:
    case DEVICE_DISPLAY:
    case DEVICE_MISC:
    case DEVICE_WIFI:
    case DEVICE_IO:
    case DEVICE_DCDC:
    case DEVICE_NONE:
    case DEVICE_HVAC:
        break;
    }
}

//device table is small. Just using a very simple and easy sort algorithm (bubble sort). At 600MHz it's going to complete in a flash
//and it only has to happen once. No need to get cute here.
void DeviceManager::sortDeviceTable()
{
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES - 1; i++)
    {
        if (devices[i] == nullptr) break;
        for (int j = i + 1; j < CFG_DEV_MGR_MAX_DEVICES; j++)
        {
            if (devices[j] == nullptr) break;
            if (devices[i]->getId() > devices[j]->getId())//swap!
            {
                Device *dev = devices[i];
                devices[i] = devices[j];
                devices[j] = dev;
            }
        }
    }
}

/*Add a new tick handler to the specified device. It should
 //technically be possible to register for multiple intervals
 //and be called for all of them but support for that is not
 //immediately necessary
 */
//void DeviceManager::addTickObserver(TickObserver *observer, uint32_t frequency) {
//}

/*Add a new filter that sends frames through to the device. There definitely has
 to be support for multiple filters per device right from the beginning.
 Mask, id, ext form the filter. canbus sets whether to attach to
 CAN0 or CAN1.
 */
//void addCanObserver(CanObserver *observer, uint32_t id, uint32_t mask, bool extended, CanHandler::CanBusNode canBus) {
//}

/*
 Send an inter-device message. Devtype has to be filled out but could be DEVICE_ANY.
 If devId is anything other than INVALID (0xFFFF) then the message will be targetted to only
 one device. Otherwise it will broadcast to any device that matches the device type (or all
 devices in the case of DEVICE_ANY).
 DeviceManager.h has a list of standard message types but you're allowed to send
 whatever you want. The standard message types are to enforce standard messages for easy
 intercommunication.
 */
void DeviceManager::sendMessage(DeviceType devType, DeviceId devId, uint32_t msgType, void* message)
{
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++)
    {
        if (devices[i] ) //does this object exist        
        {
            //if we're sending a start up message or this device is enabled then proceed
            if ( (msgType == MSG_STARTUP) || (devices[i]->isEnabled()) )
            {
                if ( (devType == DEVICE_ANY) || (devType == devices[i]->getType()) )
                {
                    if ( (devId == INVALID) || (devId == devices[i]->getId()) )
                    {
                        Logger::debug("Sending msg %x to device with ID %X (%s)", msgType, devices[i]->getId(), devices[i]->getShortName());
                        devices[i]->handleMessage(msgType, message);
                    }
                }
            }
        }
    }
}

void DeviceManager::addStatusEntry(StatusEntry entry)
{
    RecursiveScope m(statusMutex);
    statusEntries.push_back(entry);
}

//Since entries are put into the vector by copy we can't just search for this entry. We have to search for
//the name of the status entry instead.
void DeviceManager::removeStatusEntry(StatusEntry entry)
{
    removeStatusEntry(entry.statusName);
}

void DeviceManager::removeStatusEntry(String statusName)
{
    RecursiveScope m(statusMutex);
    for (std::vector<StatusEntry>::iterator it = statusEntries.begin(); it != statusEntries.end(); ) {
        if (it->statusName == statusName) it = statusEntries.erase(it);
        else ++it;
    }
}

//if a device is unloaded it'd be necessary to remove all entries it added. We do that here.
void DeviceManager::removeAllEntriesForDevice(Device *dev)
{
    RecursiveScope m(statusMutex);
    for (std::vector<StatusEntry>::iterator it = statusEntries.begin(); it != statusEntries.end(); ) {
        if (it->device == dev) it = statusEntries.erase(it);
        else ++it;
    }
}

//These copy the entry out instead of returning a pointer into statusEntries. The list can be changed
//(and reallocated) by other threads at any time so a pointer could go stale while the caller uses it.
//The copy's varPtr still points at the device's own variable which is fine as devices are never freed.
bool DeviceManager::getStatusEntryByHash(uint32_t hash, StatusEntry &entry)
{
    RecursiveScope m(statusMutex);
    for (unsigned int i = 0; i < statusEntries.size(); i++)
    {
        if (statusEntries[i].getHash() == hash)
        {
            entry = statusEntries[i];
            return true;
        }
    }
    return false;
}

bool DeviceManager::getStatusEntryByIdx(unsigned int idx, StatusEntry &entry)
{
    RecursiveScope m(statusMutex);
    if (idx < statusEntries.size())
    {
        entry = statusEntries[idx];
        return true;
    }
    return false;
}

void DeviceManager::enableAllStatusEntries()
{
    StatusCSV *statusDevice = static_cast<StatusCSV *>(getDeviceByID(STATUSCSV));
    if (!statusDevice) return;

    RecursiveScope m(statusMutex);
    for (std::vector<StatusEntry>::iterator it = statusEntries.begin(); it != statusEntries.end(); ++it) 
    {
        statusDevice->enableByHash(it->getHash());
    }
}

void DeviceManager::printAllStatusEntries()
{
    StatusCSV *statusDevice = static_cast<StatusCSV *>(getDeviceByID(STATUSCSV));
    if (!statusDevice) return;
    if (!((Device *)statusDevice)->isEnabled()) return;

    Device *dev;
    int idx = 0;
    Logger::console("All status entries:");
    RecursiveScope m(statusMutex);
    for (std::vector<StatusEntry>::iterator it = statusEntries.begin(); it != statusEntries.end(); ++it) 
    {
        idx++;
        dev = (Device *)it->device;
        if (statusDevice && statusDevice->isHashMonitored(it->getHash()))
        {
            //Logger::console("%03i. <%08x>   [%s:%s]   value: %s   <MONITORED>", 
            //    idx, it->getHash(), dev->getShortName(), it->statusName.c_str(), it->getValueAsString().c_str());
            Logger::console("%03i.   [%s:%s]   value: %s   <MONITORED>", 
                idx, dev->getShortName(), it->statusName.c_str(), it->getValueAsString().c_str());
        }
        else 
        {
            //Logger::console("%03i. <%08x>   [%s:%s]   value: %s   ", 
              //  idx, it->getHash(), dev->getShortName(), it->statusName.c_str(), it->getValueAsString().c_str());

            Logger::console("%03i.   [%s:%s]   value: %s", 
                idx, dev->getShortName(), it->statusName.c_str(), it->getValueAsString().c_str());
        }
    }
}

bool DeviceManager::addStatusObserver(Device *dev)
{
    RecursiveScope m(statusMutex);
    for (int i = 0; i < CFG_STATUS_NUM_OBSERVERS; i++)
    {
        if (!statusObservers[i] || (statusObservers[i] == dev) )
        {
            statusObservers[i] = dev;
            return true;
        }
    }
    return false;
}

bool DeviceManager::removeStatusObserver(Device *dev)
{
    RecursiveScope m(statusMutex);
    for (int i = 0; i < CFG_STATUS_NUM_OBSERVERS; i++)
    {
        if (statusObservers[i] == dev)
        {
            statusObservers[i] = nullptr;
            return true;
        }
    }
    return false;
}

//Observers are called without statusMutex held so they're free to lock their own things
//(or call back into DeviceManager) without any chance of a deadlock.
void DeviceManager::dispatchToObservers(const StatusEntry &entry)
{
    Device *observers[CFG_STATUS_NUM_OBSERVERS];
    {
        RecursiveScope m(statusMutex);
        for (int i = 0; i < CFG_STATUS_NUM_OBSERVERS; i++) observers[i] = statusObservers[i];
    }
    for (int i = 0; i < CFG_STATUS_NUM_OBSERVERS; i++)
        if (observers[i]) observers[i]->handleMessage(MSG_CONFIG_CHANGE, &entry);
}

/*
  Every tick we go through the entire list of status entries and see if their value has changed. 
  If it has we need to issue a message to registered listeners. It may bear consideration that 
  the observer callbacks should be done via a queue and not all at once instantly. This processor
  is extremely fast but some entries are going to update all of the time and thus we could end up
  with a huge number of calls here.
  Changed entries are copied out while the list is locked then dispatched after it is released.
*/
void DeviceManager::handleTick()
{
    double currVal = 0.0;
    changedEntries.clear();
    {
        RecursiveScope m(statusMutex);
        for (std::vector<StatusEntry>::iterator it = statusEntries.begin(); it != statusEntries.end(); ++it) 
        {
            currVal = it->getValueAsDouble();
            if ( fabs(currVal - it->lastValue) > 0.001) //has the value changed?
            {
                Logger::avalanche("Value of %s has changed", it->statusName.c_str());
                it->lastValue = currVal;
                changedEntries.push_back(*it);
            }
        }
    }
    for (std::vector<StatusEntry>::iterator it = changedEntries.begin(); it != changedEntries.end(); ++it)
    {
        dispatchToObservers(*it);
    }
}

void DeviceManager::setup()
{
    tickHandler.detach(this);

    Logger::info("Adding tick handler for Device Manager");

    tickHandler.attach(this, 100000ul, "DeviceManager"); //10 times per second

    //this should be large enough that you don't cause it to have to enlarge but not so large
    //that all RAM is taken up needlessly. So, tweak it in use to be big enough but not much more biggerer
    RecursiveScope m(statusMutex);
    statusEntries.reserve(200);
}

uint8_t DeviceManager::getNumThrottles() {
    return countDeviceType(DEVICE_THROTTLE);
}

uint8_t DeviceManager::getNumControllers() {
    return countDeviceType(DEVICE_MOTORCTRL);
}

uint8_t DeviceManager::getNumBMS() {
    return countDeviceType(DEVICE_BMS);
}

uint8_t DeviceManager::getNumChargers() {
    return countDeviceType(DEVICE_CHARGER);
}

uint8_t DeviceManager::getNumDisplays() {
    return countDeviceType(DEVICE_DISPLAY);
}

Throttle *DeviceManager::getAccelerator() {
    //try to find one if nothing registered. Cache it if we find one
    if (!throttle) throttle = (Throttle *)getDeviceByType(DEVICE_THROTTLE);

    //if there is no throttle then instantiate a dummy throttle
    //so down range code doesn't puke
    if (!throttle)
    {
        Logger::avalanche("getAccelerator() called but there is no registered accelerator!");
        return nullptr;
    }
    return throttle;
}

Throttle *DeviceManager::getBrake() {

    if (!brake) brake = (Throttle *)getDeviceByType(DEVICE_BRAKE);

    if (!brake)
    {
        Logger::avalanche("getBrake() called but there is no registered brake!");
        return nullptr;
    }
    return brake;
}

MotorController *DeviceManager::getMotorController() {
    if (!motorController) motorController = (MotorController *)getDeviceByType(DEVICE_MOTORCTRL);

    if (!motorController)
    {
        Logger::avalanche("getMotorController() called but there is no registered motor controller!");
        return nullptr;
    }
    return motorController;
}

/*
Allows one to request a reference to a device with the given ID. This lets code specifically request a certain
device. Normally this would be a bad idea because it sort of breaks the OOP design philosophy of polymorphism
but sometimes you can't help it.
*/
Device *DeviceManager::getDeviceByID(DeviceId id)
{
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++)
    {
        if (devices[i])
        {
            if (devices[i]->getId() == id) return devices[i];
        }
    }
    Logger::avalanche("getDeviceByID - No device with ID: %X", (int)id);
    return nullptr;
}

Device *DeviceManager::getDeviceByIdx(int idx)
{
    if (idx < 0 || idx > CFG_DEV_MGR_MAX_DEVICES) return nullptr;
    return devices[idx];
}

/*
The more object oriented version of the above function. Allows one to find the first device that matches
a given type - Usually a much better idea than most other approaches.
*/
Device *DeviceManager::getDeviceByType(DeviceType type)
{
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++)
    {
        if (devices[i] && devices[i]->isEnabled())
        {
            if (devices[i]->getType() == type) return devices[i];
        }
    }
    Logger::avalanche("getDeviceByType - No devices of type: %X", (int)type);
    return nullptr;
}

/*
 * Find the position of a device in the devices array
 * /retval the position of the device or -1 if not found.
 */
int8_t DeviceManager::findDevice(Device *device) {
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++) {
        if (device == devices[i])
            return i;
    }
    return -1;
}

/*
 * Count the number of registered devices of a certain type.
 */
uint8_t DeviceManager::countDeviceType(DeviceType deviceType) {
    uint8_t count = 0;
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++) {
        if (devices[i]->getType() == deviceType)
            count++;
    }
    return count;
}

/*
Inputs:
    settingName is a string that specifies the name of the setting to find.
    matchingDevice is a pointer to the memory location where a pointer is stored. If provided
    it will be used to update that variable to point to the Device that matched.
Outputs:
    returns a pointer to the ConfigEntry that matches. Returns nullptr if nothing matched.
*/
const ConfigEntry* DeviceManager::findConfigEntry(const char *settingName, Device **matchingDevice)
{
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++) {
        if (devices[i] && devices[i]->isEnabled()) 
        {
            const ConfigEntry *cfg = devices[i]->findConfigEntry(settingName);
            if (cfg)
            {
                if (matchingDevice) *matchingDevice = devices[i];
                return cfg;
            }
        }
    }
    return nullptr;
}

/*
Parse a value given as text and store it into the variable a ConfigEntry points to. The text is
interpreted in display units (see the precision notes in DeviceTypes.h) exactly as if it had been
typed on the serial console. If the value is stored the device saves its configuration and the
entry's afterUpdateFunc (if any) is called.
Returns 0 if the value was stored, 1 if it was below the minimum, 2 if it was above the maximum.
This saves to EEPROM so it must not be called from yield() (serialEvent and friends).
*/
int DeviceManager::applyConfigValue(Device *dev, const ConfigEntry *entry, const char *valu)
{
    uint8_t ui8;
    float fl;
    int16_t i16;
    int32_t i32;
    uint16_t ui16;
    uint32_t ui32;

    int result = 0;
    switch (entry->varType)
    {
    case CFG_ENTRY_VAR_TYPE::BYTE:
        ui8 = (uint8_t)strtol(valu, NULL, 0);
        if (ui8 < entry->minValue.u_int) result = 1;
        else if (ui8 > entry->maxValue.u_int) result = 2;
        else
        {
            *(uint8_t *)entry->varPtr = ui8;
        }
        break;
    case CFG_ENTRY_VAR_TYPE::FLOAT:
        fl = strtof(valu, NULL);
        if (fl < entry->minValue.floating) result = 1;
        else if (fl > entry->maxValue.floating) result = 2;
        else *(float *)entry->varPtr = fl;
        break;
    case CFG_ENTRY_VAR_TYPE::INT16:
        i16 = (int16_t)strtol(valu, NULL, 0);
        if (i16 < entry->minValue.s_int) result = 1;
        else if (i16 > entry->maxValue.s_int) result = 2;
        else
        {
            if (entry->precision < 0) i16 = i16 * (abs(entry->precision));
            if (entry->precision > 0 && entry->precision != 16) i16 = i16 / entry->precision;
            *(int16_t *)entry->varPtr = i16;
        }
        break;
    case CFG_ENTRY_VAR_TYPE::INT32:
        i32 = (int32_t)strtol(valu, NULL, 0);
        if (i32 < entry->minValue.s_int) result = 1;
        else if (i32 > entry->maxValue.s_int) result = 2;
        else
        {
            if (entry->precision < 0) i32 = i32 * (abs(entry->precision));
            if (entry->precision > 0 && entry->precision != 16) i32 = i32 / entry->precision;
            *(int32_t *)entry->varPtr = i32;
        }
        break;
    case CFG_ENTRY_VAR_TYPE::STRING:
        //this is an interesting one. It's easy in principle but we don't know
        //the actual size of the storage buffer so it could overwrite memory.
        //If this GEVCU thing gets popular it might be necessary to fix this
        //otherwise this is the entry point to smashing the stack for fun and profit.
        strcpy((char *)entry->varPtr, valu);
        break;
    case CFG_ENTRY_VAR_TYPE::UINT16:
        ui16 = (uint16_t)strtoul(valu, NULL, 0);
        if (ui16 < entry->minValue.u_int) result = 1;
        else if (ui16 > entry->maxValue.u_int) result = 2;
        else
        {
            if (entry->precision < 0) ui16 = ui16 * (abs(entry->precision));
            if (entry->precision > 0 && entry->precision != 16) ui16 = ui16 / entry->precision;
            *(uint16_t *)entry->varPtr = ui16;
        }
        break;
    case CFG_ENTRY_VAR_TYPE::UINT32:
        ui32 = (uint32_t)strtoul(valu, NULL, 0);
        if (ui32 < entry->minValue.u_int) result = 1;
        else if (ui32 > entry->maxValue.u_int) result = 2;
        else
        {
            if (entry->precision < 0) ui32 = ui32 * (abs(entry->precision));
            if (entry->precision > 0 && entry->precision != 16) ui32 = ui32 / entry->precision;
            *(uint32_t *)entry->varPtr = ui32;
        }
        break;
    }
    if (result == 0) //value was stored
    {
        dev->saveConfiguration(); //devices are responsible for doing their own saving and writing to eeprom
        if (entry->afterUpdateFunc)
        {
            CALL_MEMBER_FN(dev, entry->afterUpdateFunc)();
        }
    }
    return result;
}

//caller preallocates the json document and we fill it out
//send only details for given deviceID
//might need it to be enabled to actually get the details?
void DeviceManager::createJsonConfigDocForID(DynamicJsonDocument &doc, DeviceId id)
{
    Device *dev = getDeviceByID(id);
    if (dev)
    {
        __populateJsonEntry(doc, dev);
    }
}

//caller preallocates the json document and we fill it out
//send all enabled devices
void DeviceManager::createJsonConfigDoc(DynamicJsonDocument &doc)
{
    for (int j = 0; j < CFG_DEV_MGR_MAX_DEVICES; j++)
    {
        Device *dev = getDeviceByIdx(j);
        if (dev)
        {
            if (dev->isEnabled())
            {
                __populateJsonEntry(doc, dev);
            }
        }
    }
}

void DeviceManager::__populateJsonEntry(DynamicJsonDocument &doc, Device *dev)
{
    JsonObject devArr = doc.createNestedObject(dev->getShortName());
    devArr["DevID"] = dev->getId();

    const std::vector<ConfigEntry> *entries = dev->getConfigEntries();
    for (const ConfigEntry &ent : *entries)
    {
        JsonObject devEntry = devArr.createNestedObject(ent.cfgName.c_str());
        //Logger::console("%s", ent.cfgName.c_str());
        //devEntry["CfgName"] = ent.cfgName.c_str();
        devEntry["HelpTxt"] = ent.helpText.c_str();
        devEntry["Precision"] = ent.precision;
        if (ent.descFunc) devEntry["Desc"] = CALL_MEMBER_FN(dev, ent.descFunc)(); //String temporary so json copies it
        switch (ent.varType)
        {
        case CFG_ENTRY_VAR_TYPE::BYTE:
            devEntry["Valu"] =  *((uint8_t *)(ent.varPtr));
            devEntry["ValType"] = "BYTE";
            devEntry["MinValue"] = ent.minValue.u_int;
            devEntry["MaxValue"] = ent.maxValue.u_int;
            break;
        case CFG_ENTRY_VAR_TYPE::STRING:
            devEntry["Valu"] = (char *)(ent.varPtr);
            devEntry["ValType"] = "STR";
            devEntry["MinValue"] = 0;
            devEntry["MaxValue"] = 0;
            break;
        case CFG_ENTRY_VAR_TYPE::INT16:
            devEntry["Valu"] =  *((int16_t *)(ent.varPtr));
            devEntry["ValType"] = "INT16";
            devEntry["MinValue"] = ent.minValue.s_int;
            devEntry["MaxValue"] = ent.maxValue.s_int;
            break;
        case CFG_ENTRY_VAR_TYPE::UINT16:
            devEntry["Valu"] =  *((uint16_t *)(ent.varPtr));
            devEntry["ValType"] = "UINT16";
            devEntry["MinValue"] = ent.minValue.u_int;
            devEntry["MaxValue"] = ent.maxValue.u_int;
            break;
        case CFG_ENTRY_VAR_TYPE::INT32:
            devEntry["Valu"] =  *((int32_t *)(ent.varPtr));
            devEntry["ValType"] = "INT32";
            devEntry["MinValue"] = ent.minValue.s_int;
            devEntry["MaxValue"] = ent.maxValue.s_int;
            break;
        case CFG_ENTRY_VAR_TYPE::UINT32:
            devEntry["Valu"] =  *((uint32_t *)(ent.varPtr));
            devEntry["ValType"] = "UINT32";
            devEntry["MinValue"] = ent.minValue.u_int;
            devEntry["MaxValue"] = ent.maxValue.u_int;
            break;
        case CFG_ENTRY_VAR_TYPE::FLOAT:
            devEntry["Valu"] =  *((float *)(ent.varPtr));
            devEntry["ValType"] = "FLOAT";
            devEntry["MinValue"] = ent.minValue.floating;
            devEntry["MaxValue"] = ent.maxValue.floating;
            break;
        }
    }
}

void DeviceManager::createJsonDeviceList(DynamicJsonDocument &doc)
{
    Device *dev;
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++)
    {
        dev = getDeviceByIdx(i);
        if (!dev) break;
        JsonObject devEntry = doc.createNestedObject(dev->getShortName());
        devEntry["DeviceID"] = (uint16_t)dev->getId();
        devEntry["DeviceName"] = dev->getCommonName();
        devEntry["DeviceEnabled"] = dev->isEnabled();
        switch (dev->getType())
        {
        case DeviceType::DEVICE_BMS:
            devEntry["DeviceType"] = "BMS";
            break;
        case DeviceType::DEVICE_MOTORCTRL:
            devEntry["DeviceType"] = "MOTORCTRL";
            break;
        case DeviceType::DEVICE_CHARGER:
            devEntry["DeviceType"] = "CHARGER";
            break;
        case DeviceType::DEVICE_DISPLAY:
            devEntry["DeviceType"] = "DISPLAY";
            break;
        case DeviceType::DEVICE_THROTTLE:
            devEntry["DeviceType"] = "THROTTLE";
            break;
        case DeviceType::DEVICE_BRAKE:
            devEntry["DeviceType"] = "BRAKE";
            break;
        case DeviceType::DEVICE_MISC:
            devEntry["DeviceType"] = "MISC";
            break;
        case DeviceType::DEVICE_WIFI:
            devEntry["DeviceType"] = "WIFI";
            break;
        case DeviceType::DEVICE_IO:
            devEntry["DeviceType"] = "IO";
            break;
        case DeviceType::DEVICE_DCDC:
            devEntry["DeviceType"] = "DCDC";
            break;
        case DeviceType::DEVICE_HVAC:
            devEntry["DeviceType"] = "HVAC";
            break;
        case DeviceType::DEVICE_ANY:
        case DeviceType::DEVICE_NONE:
            devEntry["DeviceType"] = "ERR";
            break;
        }
    }
}

void DeviceManager::printDeviceList() {
    Logger::console("\n  ENABLED devices: (DISABLE=0xFFFF to disable where FFFF is device number)\n");
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++) {
        if (devices[i] && devices[i]->isEnabled()) {
            Logger::console("     0x%04X     %s", devices[i]->getId(), devices[i]->getCommonName());
        }
    }

    Logger::console("\n  DISABLED devices: (ENABLE=0xFFFF to enable where FFFF is device number)\n");
    for (int i = 0; i < CFG_DEV_MGR_MAX_DEVICES; i++) {
        if (devices[i] && !devices[i]->isEnabled()) {
            Logger::console("     0x%04X     %s", devices[i]->getId(), devices[i]->getCommonName());
        }
    }
}

//Create a permanent instance of the device manager useable from anywhere.
DeviceManager deviceManager;

