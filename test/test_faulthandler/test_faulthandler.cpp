/*
 * FaultHandler: the 50 entry circular fault log persisted to EEPROM at
 * EE_FAULT_LOG. Uses the real MemCache over the emulated EEPROM so
 * persistence across power cycles is exercised too.
 */
#include "gevcu_test.h"
#include "FaultHandler.h"
#include "eeprom_layout.h"

#define DEV1 0x1000
#define DEV2 0x2000

static FaultHandler *fh;

static void freshCache()
{
    delete memCache;
    memCache = new MemCache();
    memCache->setup();
}

static void bootFaultHandler()
{
    delete fh;
    fh = new FaultHandler();
    fh->setup();
}

// Flush, drop all RAM state and boot again from EEPROM.
static void powerCycle(uint32_t millisAfterBoot = 0)
{
    memCache->FlushAllPages();
    freshCache();
    hostSetMillis(millisAfterBoot);
    bootFaultHandler();
}

void setUp()
{
    hostResetAll();
    fh = nullptr;
    freshCache();
    bootFaultHandler();
    hostLogClear();
}

void tearDown()
{
    delete fh;
    fh = nullptr;
    delete memCache;
    memCache = nullptr;
}

// ------------------------------------------------------------------ setup

void test_setup_initializes_blank_eeprom()
{
    memCache->FlushAllPages();
    TEST_ASSERT_EQUAL_HEX8(0xB2, hostEepromData()[EE_FAULT_LOG]);
    TEST_ASSERT_EQUAL_UINT16(0, fh->getStoredFaultCount());
    TEST_ASSERT_EQUAL_UINT16(0, fh->getUnAckFaultCount());
    TEST_ASSERT_NULL(fh->getNextFault());
}

void test_setup_registers_tick()
{
    bool found = false;
    for (auto &a : hostTickAttachments())
        if (a.observer == fh && a.interval == CFG_TICK_INTERVAL_FAULTHANDLER) found = true;
    TEST_ASSERT_TRUE(found);
}

// ------------------------------------------------------------- raiseFault

void test_raise_records_fault()
{
    hostSetMillis(12345);
    uint16_t slot = fh->raiseFault(DEV1, 7);
    TEST_ASSERT_EQUAL_UINT16(0, slot);
    TEST_ASSERT_EQUAL_UINT16(1, fh->getStoredFaultCount());
    TEST_ASSERT_EQUAL_UINT16(1, fh->getUnAckFaultCount());
    TEST_ASSERT_TRUE(hostLogContains(4, "Fault 7 raised by device 1000"));

    FAULT *f = fh->getNextFault();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_HEX16(DEV1, f->device);
    TEST_ASSERT_EQUAL_UINT16(7, f->faultCode);
    TEST_ASSERT_EQUAL_UINT32(123, f->timeStamp); // tenths of a second
    TEST_ASSERT_TRUE(f->ongoing);
    TEST_ASSERT_FALSE(f->ack);
}

void test_ongoing_fault_is_not_duplicated()
{
    fh->raiseFault(DEV2, 1);
    TEST_ASSERT_EQUAL_UINT16(1, fh->raiseFault(DEV1, 7));
    hostLogClear();
    // Re-raising an ongoing fault reports the slot it already occupies.
    for (int i = 0; i < 10; i++) TEST_ASSERT_EQUAL_UINT16(1, fh->raiseFault(DEV1, 7));
    TEST_ASSERT_EQUAL_UINT16(2, fh->getStoredFaultCount());
    TEST_ASSERT_EQUAL_INT(0, hostLogCount(4)); // no repeated console spam
}

void test_same_code_from_different_device_is_separate()
{
    fh->raiseFault(DEV1, 7);
    fh->raiseFault(DEV2, 7);
    fh->raiseFault(DEV1, 8);
    TEST_ASSERT_EQUAL_UINT16(3, fh->getStoredFaultCount());
}

void test_cancelled_fault_can_be_raised_again()
{
    fh->raiseFault(DEV2, 1);      // slot 0
    fh->raiseFault(DEV1, 5);      // slot 1
    fh->cancelOngoingFault(DEV1, 5);
    TEST_ASSERT_FALSE(fh->getFault(1)->ongoing);
    fh->raiseFault(DEV1, 5);
    TEST_ASSERT_EQUAL_UINT16(3, fh->getStoredFaultCount());
}

void test_cancel_device_faults()
{
    fh->raiseFault(DEV2, 9);      // slot 0, keep slot 0 out of the way
    fh->raiseFault(DEV1, 1);
    fh->raiseFault(DEV1, 2);
    fh->raiseFault(DEV2, 3);
    fh->cancelDeviceFaults(DEV1);
    TEST_ASSERT_FALSE(fh->getFault(1)->ongoing);
    TEST_ASSERT_FALSE(fh->getFault(2)->ongoing);
    TEST_ASSERT_TRUE(fh->getFault(3)->ongoing);
}

void test_cancel_all_faults()
{
    for (int i = 0; i < 5; i++) fh->raiseFault(DEV1, i);
    fh->cancelAllFaults();
    for (int i = 1; i < 5; i++) TEST_ASSERT_FALSE(fh->getFault(i)->ongoing);
}

void test_log_wraps_after_history_size()
{
    for (int i = 0; i < CFG_FAULT_HISTORY_SIZE + 5; i++)
    {
        uint16_t slot = fh->raiseFault(DEV1, 100 + i);
        TEST_ASSERT_EQUAL_UINT16(i % CFG_FAULT_HISTORY_SIZE, slot);
        TEST_ASSERT_EQUAL_UINT16(100 + i, fh->getFault(slot)->faultCode);
    }
    TEST_ASSERT_EQUAL_UINT16(CFG_FAULT_HISTORY_SIZE, fh->getStoredFaultCount());
    // Slot 3 was overwritten by the 54th fault (code 153).
    TEST_ASSERT_EQUAL_UINT16(153, fh->getFault(3)->faultCode);
    TEST_ASSERT_EQUAL_UINT16(105, fh->getFault(5)->faultCode);
}

// ----------------------------------------------------------- acknowledging

void test_ack_single_fault()
{
    fh->raiseFault(DEV1, 1);
    fh->raiseFault(DEV1, 2);
    TEST_ASSERT_EQUAL_UINT16(1, fh->setFaultACK(1));
    TEST_ASSERT_EQUAL_UINT16(1, fh->getUnAckFaultCount());
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, fh->setFaultACK(CFG_FAULT_HISTORY_SIZE));
}

void test_ack_for_device()
{
    fh->raiseFault(DEV1, 1);
    fh->raiseFault(DEV2, 2);
    fh->raiseFault(DEV1, 3);
    TEST_ASSERT_EQUAL_UINT16(1, fh->setAckForDevice(DEV1));
    TEST_ASSERT_EQUAL_UINT16(1, fh->getUnAckFaultCount());
    TEST_ASSERT_EQUAL_UINT16(0, fh->setAckForDevice(DEV1)); // nothing left to ack
}

void test_ack_all()
{
    for (int i = 0; i < 4; i++) fh->raiseFault(DEV1, i);
    TEST_ASSERT_EQUAL_UINT16(1, fh->ackAllFaults());
    TEST_ASSERT_EQUAL_UINT16(0, fh->getUnAckFaultCount());
    TEST_ASSERT_EQUAL_UINT16(4, fh->getStoredFaultCount());
    TEST_ASSERT_EQUAL_UINT16(0, fh->ackAllFaults());
    TEST_ASSERT_NULL(fh->getNextFault());
}

void test_get_next_fault_visits_every_unacked_fault()
{
    fh->raiseFault(DEV1, 10);
    fh->raiseFault(DEV1, 11);
    fh->raiseFault(DEV1, 12);
    bool seen[3] = {false, false, false};
    for (int i = 0; i < 3; i++)
    {
        FAULT *f = fh->getNextFault();
        TEST_ASSERT_NOT_NULL(f);
        seen[f->faultCode - 10] = true;
    }
    TEST_ASSERT_TRUE(seen[0] && seen[1] && seen[2]);
}

void test_out_of_range_accessors()
{
    TEST_ASSERT_NULL(fh->getFault(CFG_FAULT_HISTORY_SIZE));
    TEST_ASSERT_NULL(fh->getFault(0xFFFF));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, fh->setFaultOngoing(CFG_FAULT_HISTORY_SIZE, false));
}

// -------------------------------------------------------------- persistence

void test_faults_survive_power_cycle()
{
    hostSetMillis(5000);
    fh->raiseFault(DEV1, 21);
    fh->raiseFault(DEV2, 22);
    fh->setFaultACK(1);
    powerCycle();
    TEST_ASSERT_EQUAL_UINT16(2, fh->getStoredFaultCount());
    TEST_ASSERT_EQUAL_UINT16(1, fh->getUnAckFaultCount());
    TEST_ASSERT_EQUAL_UINT16(22, fh->getFault(1)->faultCode);
    TEST_ASSERT_TRUE(fh->getFault(1)->ack);
    // Write pointer restored: next fault lands in slot 2.
    TEST_ASSERT_EQUAL_UINT16(2, fh->raiseFault(DEV1, 23));
}

void test_runtime_accumulates_across_boots()
{
    hostSetMillis(60000);   // 600 tenths
    fh->handleTick();
    powerCycle(0);
    hostSetMillis(10000);   // +100 tenths in this boot
    fh->raiseFault(DEV1, 1);
    FAULT *f = fh->getNextFault();
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_UINT32(700, f->timeStamp);
}

void test_corrupt_magic_reinitializes_log()
{
    fh->raiseFault(DEV1, 1);
    memCache->FlushAllPages();
    hostEepromData()[EE_FAULT_LOG] = 0x00;
    powerCycle();
    TEST_ASSERT_EQUAL_UINT16(0, fh->getStoredFaultCount());
}

// ------------------------------------------------ slot 0 handling (defects)

void test_first_slot_accessible()
{
    fh->raiseFault(DEV1, 1);
    TEST_ASSERT_NOT_NULL(fh->getFault(0));
    TEST_ASSERT_EQUAL_UINT16(0, fh->setFaultACK(0));
}

void test_cancel_fault_in_first_slot()
{
    fh->raiseFault(DEV1, 1);
    fh->cancelOngoingFault(DEV1, 1);
    fh->raiseFault(DEV1, 1);
    TEST_ASSERT_EQUAL_UINT16(2, fh->getStoredFaultCount());
}

void test_ack_of_first_slot_persists()
{
    fh->raiseFault(DEV1, 1);
    fh->ackAllFaults();
    powerCycle();
    TEST_ASSERT_EQUAL_UINT16(0, fh->getUnAckFaultCount());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_setup_initializes_blank_eeprom);
    RUN_TEST(test_setup_registers_tick);
    RUN_TEST(test_raise_records_fault);
    RUN_TEST(test_ongoing_fault_is_not_duplicated);
    RUN_TEST(test_same_code_from_different_device_is_separate);
    RUN_TEST(test_cancelled_fault_can_be_raised_again);
    RUN_TEST(test_cancel_device_faults);
    RUN_TEST(test_cancel_all_faults);
    RUN_TEST(test_log_wraps_after_history_size);
    RUN_TEST(test_ack_single_fault);
    RUN_TEST(test_ack_for_device);
    RUN_TEST(test_ack_all);
    RUN_TEST(test_get_next_fault_visits_every_unacked_fault);
    RUN_TEST(test_out_of_range_accessors);
    RUN_TEST(test_faults_survive_power_cycle);
    RUN_TEST(test_runtime_accumulates_across_boots);
    RUN_TEST(test_corrupt_magic_reinitializes_log);
    RUN_TEST(test_first_slot_accessible);
    RUN_TEST(test_cancel_fault_in_first_slot);
    RUN_TEST(test_ack_of_first_slot_persists);
    return UNITY_END();
}
