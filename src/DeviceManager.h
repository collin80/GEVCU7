/*
 * DeviceManager.h
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

#ifndef DEVICEMGR_H_
#define DEVICEMGR_H_

#include <vector>
#include "ArduinoJson.h"
#include "config.h"
#include "devices/io/Throttle.h"
#include "devices/motorctrl/MotorController.h"
#include "CanHandler.h"
#include "devices/Device.h"
#include "Sys_Messages.h"
#include "devices/DeviceTypes.h"

#define CALL_MEMBER_FN(object,ptrToMember)  ( ((const Device*)(object))->*(ptrToMember) )

class MotorController; // cyclic reference between MotorController and DeviceManager

class DeviceManager: public TickObserver {
public:
    DeviceManager();    // private constructor
    void addDevice(Device *device);
    void removeDevice(Device *device);
    void addStatusEntry(StatusEntry entry);
    void removeStatusEntry(StatusEntry entry);
    void removeStatusEntry(String statusName);
    void removeAllEntriesForDevice(Device *dev);
    void printAllStatusEntries();
    void sendMessage(DeviceType deviceType, DeviceId deviceId, uint32_t msgType, void* message);
    void dispatchToObservers(const StatusEntry &entry);
    bool addStatusObserver(Device *dev);
    bool removeStatusObserver(Device *dev);
    uint8_t getNumThrottles();
    uint8_t getNumControllers();
    uint8_t getNumBMS();
    uint8_t getNumChargers();
    uint8_t getNumDisplays();
    Throttle *getAccelerator();
    Throttle *getBrake();
    MotorController *getMotorController();
    Device *getDeviceByID(DeviceId);
    Device *getDeviceByType(DeviceType);
    Device *getDeviceByIdx(int idx);
    void printDeviceList();
    void updateWifi();
    Device *updateWifiByID(DeviceId);
    const ConfigEntry* findConfigEntry(const char *settingName, Device **matchingDevice);
    void handleTick();
    void setup();
    void createJsonConfigDoc(DynamicJsonDocument &doc);
    void createJsonConfigDocForID(DynamicJsonDocument &doc, DeviceId id);
    void createJsonDeviceList(DynamicJsonDocument &doc);
protected:

private:
    Device *devices[CFG_DEV_MGR_MAX_DEVICES];
    Device *statusObservers[CFG_STATUS_NUM_OBSERVERS];

    Throttle *throttle;
    Throttle *brake;
    MotorController *motorController;

    std::vector<StatusEntry> statusEntries;

    int8_t findDevice(Device *device);
    uint8_t countDeviceType(DeviceType deviceType);
    void __populateJsonEntry(DynamicJsonDocument &doc, Device *dev);
};

extern DeviceManager deviceManager;

#endif


