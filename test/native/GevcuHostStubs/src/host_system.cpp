/*
 * Link-time replacements for the system singletons whose real
 * implementations need the RTOS, CAN hardware or the whole device graph:
 * TickHandler, DeviceManager and CanHandler. They record what was asked of
 * them so tests can verify registration side effects.
 */
#include "DeviceManager.h"
#include "TickHandler.h"
#include "CanHandler.h"
#include "MemCache.h"
#include "host_test_support.h"

MemCache *memCache = nullptr;
Threads::Mutex sdAccessMutex;
bool sdCardWorking = false;

// ------------------------------------------------------------ TickHandler ---
static std::vector<HostTickAttachment> hostTicks;

void TickObserver::handleTick() {}
TickHandler::TickHandler() {}
void TickHandler::setup() {}
void TickHandler::attach(TickObserver *observer, uint32_t interval, const char *name)
{
    hostTicks.push_back({observer, interval, name ? name : ""});
}
void TickHandler::detach(TickObserver *observer)
{
    hostTicks.erase(std::remove_if(hostTicks.begin(), hostTicks.end(),
        [observer](const HostTickAttachment &a) { return a.observer == observer; }), hostTicks.end());
}
TickHandler tickHandler;

const std::vector<HostTickAttachment> &hostTickAttachments() { return hostTicks; }
void hostTickClear() { hostTicks.clear(); }

// ---------------------------------------------------------- DeviceManager ---
static std::vector<Device *> hostDevices;
static std::vector<StatusEntry> hostStatus;
static std::vector<HostSentMessage> hostMessages;

DeviceManager::DeviceManager() {}
void DeviceManager::addDevice(Device *device) { hostDevices.push_back(device); }
void DeviceManager::addStatusEntry(StatusEntry entry) { hostStatus.push_back(entry); }
void DeviceManager::sendMessage(DeviceType deviceType, DeviceId deviceId, uint32_t msgType, void *)
{
    hostMessages.push_back({(int)deviceType, deviceId, msgType});
}
void DeviceManager::handleTick() {}
DeviceManager deviceManager;

const std::vector<Device *> &hostRegisteredDevices() { return hostDevices; }
const std::vector<StatusEntry> &hostStatusEntries() { return hostStatus; }
const std::vector<HostSentMessage> &hostSentMessages() { return hostMessages; }
void hostDeviceManagerClear()
{
    hostStatus.clear();
    hostMessages.clear();
}

// ------------------------------------------------------------- CanHandler ---
CanHandler::CanHandler(CanBusNode busNumber) { canBusNode = busNumber; }
void CanHandler::detachAll(CanObserver *) {}
CanHandler canHandlerBus0(CanHandler::CAN_BUS_0);
CanHandler canHandlerBus1(CanHandler::CAN_BUS_1);
CanHandler canHandlerBus2(CanHandler::CAN_BUS_2);

// ------------------------------------------------------------------ reset ---
void hostResetAll()
{
    hostSetMillis(0);
    hostEepromErase();
    hostEepromResetCounters();
    hostEepromSetReadFailure(false);
    hostEepromSetWriteFailure(false);
    hostLogClear();
    hostTickClear();
    hostDeviceManagerClear();
    hostMutexReset();
    Serial2.hostReset();
}
