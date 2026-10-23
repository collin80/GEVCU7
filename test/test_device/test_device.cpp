/*
 * Device base class lifecycle (registration, startup/setup messages,
 * enable state, disable teardown, config lookup) and the StatusEntry helper
 * that every device uses to publish values.
 */
#include "gevcu_test.h"
#include "devices/Device.h"
#include "DeviceManager.h"
#include "TickHandler.h"
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>

#define PROBE_ID 0x0ABC

class ProbeDevice : public Device
{
public:
    ProbeDevice()
    {
        deviceId = PROBE_ID;
        deviceType = DEVICE_MISC;
    }
    void setup() override
    {
        setupCalls++;
        tickHandler.attach(this, 100000, "probe");
        ConfigEntry e = {"PROBEVAL", "A test value", &value, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr, nullptr};
        cfgEntries.push_back(e);
    }
    void loadConfiguration() override
    {
        loadCalls++;
        prefsHandler->read("ProbeVal", &value, (uint16_t)42);
    }
    void saveConfiguration() override
    {
        prefsHandler->write("ProbeVal", value);
        prefsHandler->forceCacheWrite();
    }
    PrefHandler *prefs() { return prefsHandler; }
    int setupCalls = 0;
    int loadCalls = 0;
    uint16_t value = 0;
};

static bool sentMessage(uint32_t msg, uint16_t id)
{
    for (auto &m : hostSentMessages())
        if (m.msgType == msg && m.deviceId == id) return true;
    return false;
}

static bool tickAttached(TickObserver *o)
{
    for (auto &a : hostTickAttachments())
        if (a.observer == o) return true;
    return false;
}

void setUp()
{
    hostResetAll();
    memCache = new MemCache();
    memCache->setup();
}

void tearDown()
{
    delete memCache;
    memCache = nullptr;
}

// ------------------------------------------------------------ Device life

void test_constructor_registers_with_device_manager()
{
    ProbeDevice d;
    auto &devs = hostRegisteredDevices();
    TEST_ASSERT_TRUE(std::find(devs.begin(), devs.end(), &d) != devs.end());
    TEST_ASSERT_EQUAL_HEX16(PROBE_ID, d.getId());
    TEST_ASSERT_EQUAL(DEVICE_MISC, d.getType());
    TEST_ASSERT_EQUAL_STRING("Generic Device", d.getCommonName());
    TEST_ASSERT_EQUAL_STRING("GENDEV", d.getShortName());
    TEST_ASSERT_NULL(d.getConfiguration());
}

void test_startup_message_creates_prefs_once()
{
    ProbeDevice d;
    TEST_ASSERT_NULL(d.prefs());
    d.handleMessage(MSG_STARTUP, nullptr);
    PrefHandler *p = d.prefs();
    TEST_ASSERT_NOT_NULL(p);
    d.handleMessage(MSG_STARTUP, nullptr);
    TEST_ASSERT_EQUAL_PTR(p, d.prefs());
    TEST_ASSERT_FALSE(d.isEnabled()); // new devices default to disabled
}

void test_setup_message_runs_setup_and_announces_enable()
{
    ProbeDevice d;
    d.handleMessage(MSG_STARTUP, nullptr);
    d.handleMessage(MSG_SETUP, nullptr);
    TEST_ASSERT_EQUAL_INT(1, d.setupCalls);
    TEST_ASSERT_TRUE(tickAttached(&d));
    TEST_ASSERT_TRUE(sentMessage(MSG_ENABLE, PROBE_ID));
}

void test_unrelated_messages_ignored()
{
    ProbeDevice d;
    d.handleMessage(MSG_COMMAND, nullptr);
    d.handleMessage(MSG_POWERDOWN, nullptr);
    TEST_ASSERT_EQUAL_INT(0, d.setupCalls);
    TEST_ASSERT_NULL(d.prefs());
}

void test_disable_detaches_and_announces()
{
    ProbeDevice d;
    d.handleMessage(MSG_STARTUP, nullptr);
    d.handleMessage(MSG_SETUP, nullptr);
    d.disableDevice();
    TEST_ASSERT_FALSE(tickAttached(&d));
    TEST_ASSERT_TRUE(sentMessage(MSG_DISABLE, PROBE_ID));
}

void test_force_enable_state()
{
    ProbeDevice d;
    d.forceEnableState(true); // no prefs yet: must not crash
    d.earlyInit();
    d.forceEnableState(true);
    TEST_ASSERT_TRUE(d.isEnabled());
    d.forceEnableState(false);
    TEST_ASSERT_FALSE(d.isEnabled());
}

void test_find_config_entry()
{
    ProbeDevice d;
    d.setup();
    const ConfigEntry *e = d.findConfigEntry("PROBEVAL");
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_PTR(&d.value, e->varPtr);
    TEST_ASSERT_NULL(d.findConfigEntry("PROBE"));
    TEST_ASSERT_NULL(d.findConfigEntry(""));
    TEST_ASSERT_EQUAL_UINT(1, d.getConfigEntries()->size());
}

void test_zap_configuration_restores_defaults()
{
    ProbeDevice d;
    d.earlyInit();
    d.value = 900;
    d.saveConfiguration();
    d.loadConfiguration();
    TEST_ASSERT_EQUAL_UINT16(900, d.value);
    d.zapConfiguration();
    TEST_ASSERT_EQUAL_UINT16(42, d.value);
    TEST_ASSERT_EQUAL_INT(2, d.loadCalls);
}

void test_generic_fault_descriptions()
{
    ProbeDevice d;
    TEST_ASSERT_EQUAL_STRING("No fault", d.getFaultDescription(NO_FAULT));
    TEST_ASSERT_EQUAL_STRING("CAN communications fault", d.getFaultDescription(CAN_COMM_FAULT));
    TEST_ASSERT_EQUAL_STRING("12V battery too low", d.getFaultDescription(DEVICE_12V_TOOLOW));
    // Every enum value below LAST_FAULT_CODE needs a description (catches table/enum drift).
    for (int c = 0; c < LAST_FAULT_CODE; c++) TEST_ASSERT_NOT_NULL(d.getFaultDescription(c));
    TEST_ASSERT_NULL(d.getFaultDescription(LAST_FAULT_CODE));
    TEST_ASSERT_NULL(d.getFaultDescription(0xFFFF));
}

// ------------------------------------------------------------ StatusEntry

void test_status_hash_is_fnv1a_uppercased()
{
    TEST_ASSERT_EQUAL_HEX32(0x811C9DC5, StatusEntry::fnvHash(""));
    TEST_ASSERT_EQUAL_HEX32(0xC40BF6CC, StatusEntry::fnvHash("a"));
    TEST_ASSERT_EQUAL_HEX32(0xC40BF6CC, StatusEntry::fnvHash("A"));
    TEST_ASSERT_EQUAL_HEX32(0xD18D12D4, StatusEntry::fnvHash("Throttle_Level"));
    TEST_ASSERT_EQUAL_HEX32(StatusEntry::fnvHash("THROTTLE_LEVEL"), StatusEntry::fnvHash("throttle_level"));
}

void test_status_entry_hash_cached()
{
    uint8_t v = 0;
    StatusEntry e("Motor_Temp", &v, CFG_ENTRY_VAR_TYPE::BYTE, 0, nullptr);
    TEST_ASSERT_EQUAL_HEX32(StatusEntry::fnvHash("motor_temp"), e.getHash());
    StatusEntry blank;
    blank.statusName = "Motor_Temp";
    TEST_ASSERT_EQUAL_HEX32(e.getHash(), blank.getHash()); // computed lazily
}

void test_status_value_as_string()
{
    uint8_t b = 200; int16_t i16 = -5; uint16_t u16 = 65000; int32_t i32 = -100000;
    uint32_t u32 = 4000000000u; float f = 1.5f; char str[] = "READY";
    TEST_ASSERT_EQUAL_STRING("200", StatusEntry("b", &b, BYTE, 0, nullptr).getValueAsString().c_str());
    TEST_ASSERT_EQUAL_STRING("-5", StatusEntry("i", &i16, INT16, 0, nullptr).getValueAsString().c_str());
    TEST_ASSERT_EQUAL_STRING("65000", StatusEntry("u", &u16, UINT16, 0, nullptr).getValueAsString().c_str());
    TEST_ASSERT_EQUAL_STRING("-100000", StatusEntry("I", &i32, INT32, 0, nullptr).getValueAsString().c_str());
    TEST_ASSERT_EQUAL_STRING("4000000000", StatusEntry("U", &u32, UINT32, 0, nullptr).getValueAsString().c_str());
    TEST_ASSERT_EQUAL_STRING("1.500", StatusEntry("f", &f, FLOAT, 0, nullptr).getValueAsString().c_str());
    TEST_ASSERT_EQUAL_STRING("READY", StatusEntry("s", str, STRING, 0, nullptr).getValueAsString().c_str());
}

void test_status_value_as_double()
{
    uint8_t b = 200; int16_t i16 = -5; uint16_t u16 = 65000; int32_t i32 = -100000;
    uint32_t u32 = 4000000000u; float f = 1.5f;
    TEST_ASSERT_EQUAL_DOUBLE(200.0, StatusEntry("b", &b, BYTE, 0, nullptr).getValueAsDouble());
    TEST_ASSERT_EQUAL_DOUBLE(-5.0, StatusEntry("i", &i16, INT16, 0, nullptr).getValueAsDouble());
    TEST_ASSERT_EQUAL_DOUBLE(65000.0, StatusEntry("u", &u16, UINT16, 0, nullptr).getValueAsDouble());
    TEST_ASSERT_EQUAL_DOUBLE(-100000.0, StatusEntry("I", &i32, INT32, 0, nullptr).getValueAsDouble());
    TEST_ASSERT_EQUAL_DOUBLE(4000000000.0, StatusEntry("U", &u32, UINT32, 0, nullptr).getValueAsDouble());
    TEST_ASSERT_EQUAL_DOUBLE(1.5, StatusEntry("f", &f, FLOAT, 0, nullptr).getValueAsDouble());
}

void test_status_string_value_as_double()
{
    // Run in a child process so a regression that walks off the string (crash/hang) is reported, not fatal.
    pid_t pid = fork();
    if (pid == 0)
    {
        alarm(2);
        char str[] = "AB"; // 65 + 66
        StatusEntry e("s", str, STRING, 0, nullptr);
        _exit(e.getValueAsDouble() == 131.0 ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    TEST_ASSERT_TRUE_MESSAGE(WIFEXITED(status), "getValueAsDouble(STRING) crashed or hung");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, WEXITSTATUS(status), "getValueAsDouble(STRING) returned the wrong sum");
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_constructor_registers_with_device_manager);
    RUN_TEST(test_startup_message_creates_prefs_once);
    RUN_TEST(test_setup_message_runs_setup_and_announces_enable);
    RUN_TEST(test_unrelated_messages_ignored);
    RUN_TEST(test_disable_detaches_and_announces);
    RUN_TEST(test_force_enable_state);
    RUN_TEST(test_find_config_entry);
    RUN_TEST(test_zap_configuration_restores_defaults);
    RUN_TEST(test_generic_fault_descriptions);
    RUN_TEST(test_status_hash_is_fnv1a_uppercased);
    RUN_TEST(test_status_entry_hash_cached);
    RUN_TEST(test_status_value_as_string);
    RUN_TEST(test_status_value_as_double);
    RUN_TEST(test_status_string_value_as_double);
    return UNITY_END();
}
