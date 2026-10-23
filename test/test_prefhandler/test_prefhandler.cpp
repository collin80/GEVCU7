/*
 * PrefHandler: per-device settings stored in EEPROM as
 * [FNV-1a hash of key (4)][length (1)][value] records inside a 1KB block,
 * plus the device table at EE_DEVICE_TABLE that assigns those blocks.
 * Runs on top of the real MemCache and the emulated EEPROM.
 */
#include "gevcu_test.h"
#include "PrefHandler.h"

#define DEV_A 0x2222
#define DEV_B 0x3333

static void freshCache()
{
    delete memCache;
    memCache = new MemCache();
    memCache->setup();
}

// Simulates a power cycle: flush everything, then come back with a cold cache.
static void reboot()
{
    memCache->FlushAllPages();
    freshCache();
}

static uint16_t tableEntry(int pos)
{
    uint16_t v;
    hostEepromPeek(EE_DEVICE_TABLE + 2 * pos, &v, 2);
    return v;
}

static uint32_t blockBase(int pos) { return EE_DEVICES_BASE + EE_DEVICE_SIZE * pos; }

void setUp()
{
    hostResetAll();
    freshCache();
}

void tearDown()
{
    delete memCache;
    memCache = nullptr;
}

// ------------------------------------------------------------ device table

void test_blank_eeprom_initializes_device_table()
{
    PrefHandler p(DEV_A);
    memCache->FlushAllPages();
    TEST_ASSERT_EQUAL_HEX16(0xDEAD, tableEntry(0));
    TEST_ASSERT_EQUAL_HEX16(AUTO_ENABLE_DEV1 | 0x8000, tableEntry(1));
    TEST_ASSERT_EQUAL_HEX16(AUTO_ENABLE_DEV2 | 0x8000, tableEntry(2));
    TEST_ASSERT_EQUAL_HEX16(AUTO_ENABLE_DEV3 | 0x8000, tableEntry(3));
    TEST_ASSERT_TRUE(hostLogContains(5, "Initializing EEPROM device table"));
}

void test_auto_enabled_device_is_found_and_enabled()
{
    PrefHandler p(AUTO_ENABLE_DEV1);
    TEST_ASSERT_TRUE(p.isEnabled());
}

void test_new_device_gets_next_free_slot_disabled()
{
    PrefHandler p(DEV_A);
    memCache->FlushAllPages();
    TEST_ASSERT_FALSE(p.isEnabled());
    TEST_ASSERT_EQUAL_HEX16(DEV_A, tableEntry(4)); // slots 1..3 hold the auto-enabled devices
    uint16_t storedId;
    hostEepromPeek(blockBase(4) + EE_DEVICE_ID, &storedId, 2);
    TEST_ASSERT_EQUAL_HEX16(DEV_A, storedId);
}

void test_same_device_reuses_its_slot()
{
    {
        PrefHandler p(DEV_A);
        p.write("Speed", (uint16_t)1234);
    }
    PrefHandler again(DEV_A);
    uint16_t v;
    again.read("Speed", &v, 0);
    TEST_ASSERT_EQUAL_UINT16(1234, v);
    memCache->FlushAllPages();
    TEST_ASSERT_EQUAL_HEX16(0, tableEntry(5)); // no second slot allocated
}

void test_devices_have_independent_settings()
{
    PrefHandler a(DEV_A), b(DEV_B);
    a.write("Value", (uint32_t)111);
    b.write("Value", (uint32_t)222);
    uint32_t va, vb;
    a.read("Value", &va, 0);
    b.read("Value", &vb, 0);
    TEST_ASSERT_EQUAL_UINT32(111, va);
    TEST_ASSERT_EQUAL_UINT32(222, vb);
}

void test_valid_table_is_not_reinitialized()
{
    { PrefHandler p(DEV_A); }
    reboot();
    hostLogClear();
    PrefHandler p(DEV_B);
    TEST_ASSERT_FALSE(hostLogContains(5, "Initializing EEPROM device table"));
    memCache->FlushAllPages();
    TEST_ASSERT_EQUAL_HEX16(DEV_A, tableEntry(4));
    TEST_ASSERT_EQUAL_HEX16(DEV_B, tableEntry(5));
}

void test_set_device_status_enables_after_reboot()
{
    { PrefHandler p(DEV_A); }
    TEST_ASSERT_TRUE(PrefHandler::setDeviceStatus(DEV_A, true));
    reboot();
    PrefHandler p(DEV_A);
    TEST_ASSERT_TRUE(p.isEnabled());

    TEST_ASSERT_TRUE(PrefHandler::setDeviceStatus(DEV_A, false));
    reboot();
    PrefHandler q(DEV_A);
    TEST_ASSERT_FALSE(q.isEnabled());
}

void test_set_device_status_unknown_device()
{
    { PrefHandler p(DEV_A); }
    TEST_ASSERT_FALSE(PrefHandler::setDeviceStatus(0x4444, true));
}

void test_device_table_full_is_reported()
{
    // Slots 1..59 exist; three are taken by the auto-enabled devices.
    const int freeSlots = CFG_DEV_MGR_MAX_DEVICES - 1 - 3;
    for (int i = 0; i < freeSlots; i++) PrefHandler p(0x5000 + i);
    TEST_ASSERT_FALSE(hostLogContains(4, "Device Table Full"));
    PrefHandler overflow(0x6000);
    TEST_ASSERT_TRUE(hostLogContains(4, "Device Table Full"));
}

void test_device_table_full_does_not_corrupt_other_devices()
{
    const int freeSlots = CFG_DEV_MGR_MAX_DEVICES - 1 - 3;
    PrefHandler *last = nullptr;
    for (int i = 0; i < freeSlots; i++)
    {
        delete last;
        last = new PrefHandler(0x5000 + i);
    }
    char key[16];
    for (int k = 0; k < 10; k++) { snprintf(key, sizeof(key), "K%d", k); last->write(key, (uint32_t)k); }
    last->saveChecksum();
    TEST_ASSERT_TRUE(last->checksumValid());

    PrefHandler overflow(0x6000);
    for (int k = 0; k < 30; k++) { snprintf(key, sizeof(key), "X%d", k); overflow.write(key, (uint32_t)0x12345678); }
    TEST_ASSERT_TRUE_MESSAGE(last->checksumValid(), "last device's block was modified by the overflow device");
    delete last;
}

// ------------------------------------------------------- settings storage

void test_missing_keys_return_defaults()
{
    PrefHandler p(DEV_A);
    uint8_t u8; uint16_t u16; uint32_t u32; float f; double d; char s[32];
    TEST_ASSERT_TRUE(p.read("nope8", &u8, (uint8_t)7));
    TEST_ASSERT_TRUE(p.read("nope16", &u16, (uint16_t)700));
    TEST_ASSERT_TRUE(p.read("nope32", &u32, (uint32_t)70000));
    TEST_ASSERT_TRUE(p.read("nopef", &f, 1.5f));
    TEST_ASSERT_TRUE(p.read("noped", &d, 2.25));
    TEST_ASSERT_TRUE(p.read("nopes", s, "dflt"));
    TEST_ASSERT_EQUAL_UINT8(7, u8);
    TEST_ASSERT_EQUAL_UINT16(700, u16);
    TEST_ASSERT_EQUAL_UINT32(70000, u32);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, f);
    TEST_ASSERT_EQUAL_DOUBLE(2.25, d);
    TEST_ASSERT_EQUAL_STRING("dflt", s);
}

void test_typed_round_trips()
{
    PrefHandler p(DEV_A);
    TEST_ASSERT_TRUE(p.write("u8", (uint8_t)200));
    TEST_ASSERT_TRUE(p.write("u16", (uint16_t)60000));
    TEST_ASSERT_TRUE(p.write("u32", (uint32_t)4000000000u));
    TEST_ASSERT_TRUE(p.write("flt", -12.5f));
    TEST_ASSERT_TRUE(p.write("dbl", 3.14159265358979));
    uint8_t u8; uint16_t u16; uint32_t u32; float f; double d;
    p.read("u8", &u8, 0);
    p.read("u16", &u16, 0);
    p.read("u32", &u32, 0);
    p.read("flt", &f, 0.0f);
    p.read("dbl", &d, 0.0);
    TEST_ASSERT_EQUAL_UINT8(200, u8);
    TEST_ASSERT_EQUAL_UINT16(60000, u16);
    TEST_ASSERT_EQUAL_UINT32(4000000000u, u32);
    TEST_ASSERT_EQUAL_FLOAT(-12.5f, f);
    TEST_ASSERT_EQUAL_DOUBLE(3.14159265358979, d);
}

void test_overwrite_keeps_single_record()
{
    PrefHandler p(DEV_A);
    p.write("Torque", (uint16_t)1);
    p.write("Torque", (uint16_t)2);
    p.write("Next", (uint16_t)3);
    uint16_t t, n;
    p.read("Torque", &t, 0);
    p.read("Next", &n, 0);
    TEST_ASSERT_EQUAL_UINT16(2, t);
    TEST_ASSERT_EQUAL_UINT16(3, n);
    // Second record must start right after the first: 20 + 4 + 1 + 2
    memCache->FlushAllPages();
    uint8_t len;
    hostEepromPeek(blockBase(4) + SETTINGS_START + 4, &len, 1);
    TEST_ASSERT_EQUAL_UINT8(2, len);
    hostEepromPeek(blockBase(4) + SETTINGS_START + 7 + 4, &len, 1);
    TEST_ASSERT_EQUAL_UINT8(2, len);
}

void test_keys_are_case_insensitive()
{
    PrefHandler p(DEV_A);
    p.write("MaxRPM", (uint16_t)9000);
    uint16_t v;
    p.read("MAXRPM", &v, 0);
    TEST_ASSERT_EQUAL_UINT16(9000, v);
    p.read("maxrpm", &v, 0);
    TEST_ASSERT_EQUAL_UINT16(9000, v);
}

void test_size_mismatch_rejected()
{
    PrefHandler p(DEV_A);
    TEST_ASSERT_TRUE(p.write("Mode", (uint8_t)3));
    hostLogClear();
    TEST_ASSERT_FALSE(p.write("Mode", (uint16_t)300));
    TEST_ASSERT_TRUE(hostLogContains(4, "improper length"));
    uint8_t v;
    p.read("Mode", &v, 0);
    TEST_ASSERT_EQUAL_UINT8(3, v);
    TEST_ASSERT_FALSE(p.write("Mode", (uint32_t)1));
    TEST_ASSERT_FALSE(p.write("Mode", 1.0));
}

void test_string_round_trip()
{
    PrefHandler p(DEV_A);
    TEST_ASSERT_TRUE(p.write("Name", "GEVCU7", 20));
    char buf[32];
    p.read("Name", buf, "");
    TEST_ASSERT_EQUAL_STRING("GEVCU7", buf);
    TEST_ASSERT_TRUE(p.write("Name", "Electric", 20));
    p.read("Name", buf, "");
    TEST_ASSERT_EQUAL_STRING("Electric", buf);
}

void test_string_does_not_disturb_following_record()
{
    PrefHandler p(DEV_A);
    p.write("Name", "a much longer value", 20);
    p.write("After", (uint32_t)0x01020304);
    p.write("Name", "short", 20);
    uint32_t v;
    p.read("After", &v, 0);
    TEST_ASSERT_EQUAL_HEX32(0x01020304, v);
}

void test_string_truncated_to_maxlen()
{
    PrefHandler p(DEV_A);
    p.write("Short", "abcdefghij", 4);
    p.write("Next", (uint32_t)0x41414141); // 'AAAA', no NUL bytes
    char buf[64];
    p.read("Short", buf, "");
    TEST_ASSERT_EQUAL_STRING("abcd", buf);
}

void test_block_round_trip()
{
    PrefHandler p(DEV_A);
    uint8_t in[40], out[40];
    for (int i = 0; i < 40; i++) in[i] = (uint8_t)(i ^ 0x5A);
    TEST_ASSERT_TRUE(p.writeBlock("Cal", in, sizeof(in)));
    TEST_ASSERT_TRUE(p.readBlock("Cal", out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, sizeof(in));
    TEST_ASSERT_FALSE(p.writeBlock("Cal", in, 10)); // different size
}

void test_read_block_missing_key_fails()
{
    PrefHandler p(DEV_A);
    uint8_t out[4];
    TEST_ASSERT_FALSE(p.readBlock("Nothing", out, sizeof(out)));
}

void test_settings_survive_reboot()
{
    {
        PrefHandler p(DEV_A);
        p.write("Kp", 0.75f);
        p.write("Limit", (uint16_t)450);
        p.write("Label", "Front", 10);
        p.forceCacheWrite();
    }
    freshCache(); // no extra flush: forceCacheWrite() alone must persist
    PrefHandler p(DEV_A);
    float kp; uint16_t limit; char label[16];
    p.read("Kp", &kp, 0.0f);
    p.read("Limit", &limit, 0);
    p.read("Label", label, "");
    TEST_ASSERT_EQUAL_FLOAT(0.75f, kp);
    TEST_ASSERT_EQUAL_UINT16(450, limit);
    TEST_ASSERT_EQUAL_STRING("Front", label);
}

void test_lkg_storage_is_separate()
{
    PrefHandler p(DEV_A);
    p.write("Gain", (uint16_t)10);
    p.LKG_mode(PREF_MODE_LKG);
    uint16_t v;
    p.read("Gain", &v, 99);
    TEST_ASSERT_EQUAL_UINT16(99, v);
    p.write("Gain", (uint16_t)20);
    p.LKG_mode(PREF_MODE_NORMAL);
    p.read("Gain", &v, 0);
    TEST_ASSERT_EQUAL_UINT16(10, v);
    p.LKG_mode(PREF_MODE_LKG);
    p.read("Gain", &v, 0);
    TEST_ASSERT_EQUAL_UINT16(20, v);
}

void test_reset_eeprom_restores_defaults()
{
    PrefHandler p(DEV_A);
    p.write("A", (uint16_t)1);
    p.write("B", (uint32_t)2);
    p.resetEEPROM();
    uint16_t a; uint32_t b;
    p.read("A", &a, 55);
    p.read("B", &b, 66);
    TEST_ASSERT_EQUAL_UINT16(55, a);
    TEST_ASSERT_EQUAL_UINT32(66, b);
    // The 20 byte header (checksum + device id) is preserved
    uint16_t storedId;
    hostEepromPeek(blockBase(4) + EE_DEVICE_ID, &storedId, 2);
    TEST_ASSERT_EQUAL_HEX16(DEV_A, storedId);
}

void test_block_full_fails_cleanly()
{
    PrefHandler p(DEV_A);
    char key[16];
    int stored = 0;
    for (int i = 0; i < 200; i++)
    {
        snprintf(key, sizeof(key), "Entry%d", i);
        if (!p.write(key, (uint32_t)i)) break;
        stored++;
    }
    TEST_ASSERT_TRUE(stored > 100);   // 9 bytes per uint32 record in a ~1000 byte area
    TEST_ASSERT_TRUE(stored < 200);   // and it must eventually refuse
}

void test_block_full_does_not_spill_into_next_device()
{
    PrefHandler a(DEV_A), b(DEV_B);
    b.saveChecksum();
    memCache->FlushAllPages();
    uint8_t before[SETTINGS_START];
    hostEepromPeek(blockBase(5), before, sizeof(before));

    char key[16];
    for (int i = 0; i < 200; i++)
    {
        snprintf(key, sizeof(key), "Entry%d", i);
        if (!a.write(key, (uint32_t)0xA5A5A5A5)) break;
    }
    memCache->FlushAllPages();
    uint8_t after[SETTINGS_START];
    hostEepromPeek(blockBase(5), after, sizeof(after));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(before, after, sizeof(before));
}

void test_erase_missing_key()
{
    PrefHandler p(DEV_A);
    TEST_ASSERT_FALSE(p.eraseByKey("NotThere"));
}

void test_erase_by_key_removes_setting()
{
    PrefHandler p(DEV_A);
    p.write("Temp", (uint16_t)77);
    TEST_ASSERT_TRUE(p.eraseByKey("Temp"));
    uint16_t v;
    p.read("Temp", &v, 5);
    TEST_ASSERT_EQUAL_UINT16(5, v);
}

// --------------------------------------------------------------- checksum

void test_checksum_round_trip()
{
    PrefHandler p(DEV_A);
    p.write("X", (uint16_t)1234);
    p.saveChecksum();
    TEST_ASSERT_TRUE(p.checksumValid());
}

void test_checksum_detects_change()
{
    PrefHandler p(DEV_A);
    p.write("X", (uint16_t)1234);
    p.saveChecksum();
    p.write("X", (uint16_t)1235);
    TEST_ASSERT_FALSE(p.checksumValid());
    TEST_ASSERT_TRUE(hostLogContains(4, "Checksum didn't match"));
}

void test_checksum_detects_eeprom_corruption()
{
    PrefHandler p(DEV_A);
    p.write("X", (uint32_t)0x11111111);
    p.saveChecksum();
    memCache->FlushAllPages();
    hostEepromData()[blockBase(4) + 500] ^= 0x01;
    freshCache();
    PrefHandler q(DEV_A);
    TEST_ASSERT_FALSE(q.checksumValid());
}

void test_checksum_detects_wrong_device_id()
{
    PrefHandler p(DEV_A);
    memCache->FlushAllPages();
    uint16_t bogus = DEV_B;
    hostEepromPoke(blockBase(4) + EE_DEVICE_ID, &bogus, 2);
    freshCache();
    PrefHandler q(DEV_A);
    q.saveChecksum(); // checksum itself is fine, only the id is wrong
    TEST_ASSERT_FALSE(q.checksumValid());
    TEST_ASSERT_TRUE(hostLogContains(4, "ID mismatch"));
}

void test_checksum_repairs_missing_device_id()
{
    PrefHandler p(DEV_A);
    memCache->FlushAllPages();
    uint16_t blank = 0xFFFF;
    hostEepromPoke(blockBase(4) + EE_DEVICE_ID, &blank, 2);
    freshCache();
    PrefHandler q(DEV_A);
    q.saveChecksum();
    TEST_ASSERT_TRUE(q.checksumValid());
    memCache->FlushAllPages();
    uint16_t storedId;
    hostEepromPeek(blockBase(4) + EE_DEVICE_ID, &storedId, 2);
    TEST_ASSERT_EQUAL_HEX16(DEV_A, storedId);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_blank_eeprom_initializes_device_table);
    RUN_TEST(test_auto_enabled_device_is_found_and_enabled);
    RUN_TEST(test_new_device_gets_next_free_slot_disabled);
    RUN_TEST(test_same_device_reuses_its_slot);
    RUN_TEST(test_devices_have_independent_settings);
    RUN_TEST(test_valid_table_is_not_reinitialized);
    RUN_TEST(test_set_device_status_enables_after_reboot);
    RUN_TEST(test_set_device_status_unknown_device);
    RUN_TEST(test_device_table_full_is_reported);
    RUN_TEST(test_device_table_full_does_not_corrupt_other_devices);
    RUN_TEST(test_missing_keys_return_defaults);
    RUN_TEST(test_typed_round_trips);
    RUN_TEST(test_overwrite_keeps_single_record);
    RUN_TEST(test_keys_are_case_insensitive);
    RUN_TEST(test_size_mismatch_rejected);
    RUN_TEST(test_string_round_trip);
    RUN_TEST(test_string_does_not_disturb_following_record);
    RUN_TEST(test_string_truncated_to_maxlen);
    RUN_TEST(test_block_round_trip);
    RUN_TEST(test_read_block_missing_key_fails);
    RUN_TEST(test_settings_survive_reboot);
    RUN_TEST(test_lkg_storage_is_separate);
    RUN_TEST(test_reset_eeprom_restores_defaults);
    RUN_TEST(test_block_full_fails_cleanly);
    RUN_TEST(test_block_full_does_not_spill_into_next_device);
    RUN_TEST(test_erase_missing_key);
    RUN_TEST(test_erase_by_key_removes_setting);
    RUN_TEST(test_checksum_round_trip);
    RUN_TEST(test_checksum_detects_change);
    RUN_TEST(test_checksum_detects_eeprom_corruption);
    RUN_TEST(test_checksum_detects_wrong_device_id);
    RUN_TEST(test_checksum_repairs_missing_device_id);
    return UNITY_END();
}
