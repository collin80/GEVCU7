/*
 * MemCache: the write-back page cache in front of the 256KB I2C EEPROM.
 * Runs the real MemCache against an emulated 24-series EEPROM on Wire, so
 * these tests check what actually lands in the chip and when.
 */
#include "gevcu_test.h"
#include "MemCache.h"

static MemCache *cache;

static uint8_t eepromByte(uint32_t addr) { return hostEepromData()[addr]; }

void setUp()
{
    hostResetAll();
    cache = new MemCache();
    cache->setup();
    hostEepromResetCounters();
}

void tearDown()
{
    delete cache;
    cache = nullptr;
}

static void tick(int n)
{
    for (int i = 0; i < n; i++) cache->handleTick();
}

// ------------------------------------------------------------------ basics

void test_setup_registers_tick_handler()
{
    bool found = false;
    for (auto &a : hostTickAttachments())
        if (a.observer == cache && a.interval == CFG_TICK_INTERVAL_MEM_CACHE) found = true;
    TEST_ASSERT_TRUE(found);
}

void test_reads_blank_eeprom_as_ff()
{
    uint32_t v = 0;
    TEST_ASSERT_TRUE(cache->Read(1234, &v));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, v);
}

void test_reads_existing_eeprom_contents()
{
    const uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    hostEepromPoke(0x2010, data, 4);
    uint32_t v = 0;
    TEST_ASSERT_TRUE(cache->Read(0x2010, &v));
    TEST_ASSERT_EQUAL_HEX32(0x44332211, v); // little endian
}

void test_typed_round_trips()
{
    uint8_t u8 = 0xA5; uint16_t u16 = 0xBEEF; uint32_t u32 = 0xCAFEBABE;
    float f = -3.25f; double d = 1234.5678;
    TEST_ASSERT_TRUE(cache->Write(100, u8));
    TEST_ASSERT_TRUE(cache->Write(101, u16));
    TEST_ASSERT_TRUE(cache->Write(103, u32));
    TEST_ASSERT_TRUE(cache->Write(107, f));
    TEST_ASSERT_TRUE(cache->Write(111, d));

    uint8_t r8; uint16_t r16; uint32_t r32; float rf; double rd;
    TEST_ASSERT_TRUE(cache->Read(100, &r8));
    TEST_ASSERT_TRUE(cache->Read(101, &r16));
    TEST_ASSERT_TRUE(cache->Read(103, &r32));
    TEST_ASSERT_TRUE(cache->Read(107, &rf));
    TEST_ASSERT_TRUE(cache->Read(111, &rd));
    TEST_ASSERT_EQUAL_HEX8(u8, r8);
    TEST_ASSERT_EQUAL_HEX16(u16, r16);
    TEST_ASSERT_EQUAL_HEX32(u32, r32);
    TEST_ASSERT_EQUAL_FLOAT(f, rf);
    TEST_ASSERT_EQUAL_DOUBLE(d, rd);
}

void test_block_round_trip()
{
    uint8_t in[300], out[300];
    for (int i = 0; i < 300; i++) in[i] = (uint8_t)(i * 3 + 1);
    TEST_ASSERT_TRUE(cache->Write(5000, in, sizeof(in)));
    memset(out, 0, sizeof(out));
    TEST_ASSERT_TRUE(cache->Read(5000, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, sizeof(in));
}

// ------------------------------------------------------ write-back behaviour

void test_writes_are_deferred_until_flush()
{
    cache->Write(0x400, (uint32_t)0x12345678);
    TEST_ASSERT_EQUAL_HEX8(0xFF, eepromByte(0x400));
    TEST_ASSERT_EQUAL_UINT32(0, hostEepromPageWrites());

    cache->FlushAllPages();
    TEST_ASSERT_EQUAL_HEX8(0x78, eepromByte(0x400));
    TEST_ASSERT_EQUAL_HEX8(0x12, eepromByte(0x403));
    TEST_ASSERT_EQUAL_UINT32(1, hostEepromPageWrites());
}

void test_flush_preserves_rest_of_page()
{
    uint8_t original[256];
    for (int i = 0; i < 256; i++) original[i] = (uint8_t)i;
    hostEepromPoke(0x800, original, 256);
    cache->Write(0x810, (uint8_t)0xEE);
    cache->FlushAllPages();
    for (int i = 0; i < 256; i++)
    {
        uint8_t expect = (i == 0x10) ? 0xEE : (uint8_t)i;
        TEST_ASSERT_EQUAL_HEX8(expect, eepromByte(0x800 + i));
    }
}

void test_flush_skips_clean_pages()
{
    uint8_t v;
    cache->Read(0x100, &v);
    cache->Read(0x900, &v);
    cache->FlushAllPages();
    TEST_ASSERT_EQUAL_UINT32(0, hostEepromPageWrites());
}

void test_repeated_flush_writes_once()
{
    cache->Write(0x100, (uint8_t)1);
    cache->FlushAllPages();
    cache->FlushAllPages();
    TEST_ASSERT_EQUAL_UINT32(1, hostEepromPageWrites());
}

void test_write_spanning_two_pages()
{
    cache->Write(0x1FE, (uint32_t)0xA1B2C3D4);
    cache->FlushAllPages();
    TEST_ASSERT_EQUAL_HEX8(0xD4, eepromByte(0x1FE));
    TEST_ASSERT_EQUAL_HEX8(0xC3, eepromByte(0x1FF));
    TEST_ASSERT_EQUAL_HEX8(0xB2, eepromByte(0x200));
    TEST_ASSERT_EQUAL_HEX8(0xA1, eepromByte(0x201));
    TEST_ASSERT_EQUAL_UINT32(2, hostEepromPageWrites());
}

void test_upper_banks_are_addressed_correctly()
{
    // Addresses >= 64KB select I2C device 0x51..0x53 via the upper address bits.
    const uint32_t addrs[] = {0x0FF00, 0x10000, 0x2ABCD, 0x3FFFC};
    for (uint32_t a : addrs) cache->Write(a, (uint32_t)(a ^ 0x5A5A5A5A));
    cache->FlushAllPages();
    for (uint32_t a : addrs)
    {
        uint32_t v;
        hostEepromPeek(a, &v, 4);
        TEST_ASSERT_EQUAL_HEX32(a ^ 0x5A5A5A5A, v);
    }
}

void test_flush_address_only_flushes_that_page()
{
    cache->Write(0x100, (uint8_t)1);
    cache->Write(0x300, (uint8_t)2);
    cache->FlushAddress(0x1AB);
    TEST_ASSERT_EQUAL_HEX8(1, eepromByte(0x100));
    TEST_ASSERT_EQUAL_HEX8(0xFF, eepromByte(0x300));
}

// ------------------------------------------------------------------- aging

void test_dirty_page_flushed_after_full_aging_period()
{
    cache->Write(0x500, (uint8_t)0x42);
    const int ticksToFlush = AGING_PERIOD * MAX_AGE;
    tick(ticksToFlush - 1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, eepromByte(0x500));
    tick(1);
    TEST_ASSERT_EQUAL_HEX8(0x42, eepromByte(0x500));
}

void test_age_fully_flushes_on_next_tick()
{
    cache->Write(0x600, (uint8_t)0x24);
    cache->AgeFullyAddress(0x6FF);
    TEST_ASSERT_EQUAL_HEX8(0xFF, eepromByte(0x600));
    tick(1);
    TEST_ASSERT_EQUAL_HEX8(0x24, eepromByte(0x600));
}

void test_one_aged_page_flushed_per_tick()
{
    cache->Write(0x100, (uint8_t)1);
    cache->Write(0x200, (uint8_t)2);
    cache->AgeFullyAddress(0x100);
    cache->AgeFullyAddress(0x200);
    tick(1);
    TEST_ASSERT_EQUAL_UINT32(1, hostEepromPageWrites());
    tick(1);
    TEST_ASSERT_EQUAL_UINT32(2, hostEepromPageWrites());
}

void test_age_fully_on_uncached_address_is_harmless()
{
    cache->AgeFullyAddress(0x7700);
    tick(1);
    TEST_ASSERT_EQUAL_UINT32(0, hostEepromPageWrites());
}

void test_clean_pages_never_written_by_aging()
{
    uint8_t v;
    cache->Read(0x100, &v);
    tick(AGING_PERIOD * MAX_AGE + 5);
    TEST_ASSERT_EQUAL_UINT32(0, hostEepromPageWrites());
}

// ------------------------------------------------------------ invalidation

void test_invalidate_writes_dirty_page_then_rereads()
{
    cache->Write(0x700, (uint8_t)0x10);
    cache->InvalidateAddress(0x700);
    TEST_ASSERT_EQUAL_HEX8(0x10, eepromByte(0x700));

    // Change the chip behind the cache's back; the next read must see it.
    uint8_t outside = 0x99;
    hostEepromPoke(0x700, &outside, 1);
    uint8_t v;
    cache->Read(0x700, &v);
    TEST_ASSERT_EQUAL_HEX8(0x99, v);
}

void test_cached_page_hides_external_changes_until_invalidated()
{
    uint8_t v;
    cache->Read(0x900, &v);
    uint8_t outside = 0x55;
    hostEepromPoke(0x900, &outside, 1);
    cache->Read(0x900, &v);
    TEST_ASSERT_EQUAL_HEX8(0xFF, v);
    cache->InvalidateAll();
    cache->Read(0x900, &v);
    TEST_ASSERT_EQUAL_HEX8(0x55, v);
}

void test_invalidate_all_persists_every_dirty_page()
{
    for (uint32_t p = 0; p < 10; p++) cache->Write(p * 256 + 7, (uint8_t)(p + 1));
    cache->InvalidateAll();
    for (uint32_t p = 0; p < 10; p++) TEST_ASSERT_EQUAL_HEX8(p + 1, eepromByte(p * 256 + 7));
}

void test_invalidate_page_out_of_range_is_ignored()
{
    cache->InvalidatePage(NUM_CACHED_PAGES);
    cache->InvalidatePage(255);
    TEST_PASS();
}

// ----------------------------------------------------------------- eviction

void test_evicts_clean_pages_without_writing()
{
    uint8_t v;
    for (uint32_t p = 0; p < NUM_CACHED_PAGES + 20; p++) TEST_ASSERT_TRUE(cache->Read(p * 256, &v));
    TEST_ASSERT_EQUAL_UINT32(0, hostEepromPageWrites());
}

void test_cache_full_of_dirty_pages_evicts_by_flushing()
{
    const uint32_t pagesUsed = NUM_CACHED_PAGES + 8;
    for (uint32_t p = 0; p < pagesUsed; p++)
        TEST_ASSERT_TRUE(cache->Write(p * 256 + 3, (uint8_t)(p & 0xFF)));
    // Nothing may be lost: every value must be readable whether it is still
    // cached or was flushed to make room.
    for (uint32_t p = 0; p < pagesUsed; p++)
    {
        uint8_t v;
        TEST_ASSERT_TRUE(cache->Read(p * 256 + 3, &v));
        TEST_ASSERT_EQUAL_HEX8(p & 0xFF, v);
    }
    TEST_ASSERT_TRUE(hostEepromPageWrites() >= 8);
    cache->FlushAllPages();
    for (uint32_t p = 0; p < pagesUsed; p++) TEST_ASSERT_EQUAL_HEX8(p & 0xFF, eepromByte(p * 256 + 3));
}

void test_data_survives_a_new_cache_instance()
{
    cache->Write(0x1234, (uint32_t)0x0BADF00D);
    cache->FlushAllPages();
    delete cache;
    cache = new MemCache();
    cache->setup();
    uint32_t v;
    cache->Read(0x1234, &v);
    TEST_ASSERT_EQUAL_HEX32(0x0BADF00D, v);
}

// -------------------------------------------------------------- concurrency

void test_mutex_not_reentered()
{
    cache->Write(0x100, (uint8_t)1);
    cache->AgeFullyAddress(0x100);
    tick(1);
    cache->Write(0x200, (uint8_t)1);
    cache->FlushAllPages();
    TEST_ASSERT_TRUE(hostMutexMaxDepth() <= 1);
}

// --------------------------------------------------------- I2C error paths

void test_read_failure_is_reported()
{
    cache->Write(0x100, (uint8_t)0x77);
    cache->InvalidateAll();
    uint8_t v;
    cache->Read(0x100, &v); // slot 0 now holds page 0x100's data
    cache->InvalidateAll();
    hostEepromSetReadFailure(true);
    bool ok = cache->Read(0x5000, &v);
    TEST_ASSERT_FALSE_MESSAGE(ok, "Read() claimed success although the EEPROM returned nothing");
}

void test_write_failure_keeps_page_dirty()
{
    cache->Write(0x100, (uint8_t)0x31);
    hostEepromSetWriteFailure(true);
    cache->FlushAllPages();
    hostEepromSetWriteFailure(false);
    cache->FlushAllPages();
    TEST_ASSERT_EQUAL_HEX8(0x31, eepromByte(0x100));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_setup_registers_tick_handler);
    RUN_TEST(test_reads_blank_eeprom_as_ff);
    RUN_TEST(test_reads_existing_eeprom_contents);
    RUN_TEST(test_typed_round_trips);
    RUN_TEST(test_block_round_trip);
    RUN_TEST(test_writes_are_deferred_until_flush);
    RUN_TEST(test_flush_preserves_rest_of_page);
    RUN_TEST(test_flush_skips_clean_pages);
    RUN_TEST(test_repeated_flush_writes_once);
    RUN_TEST(test_write_spanning_two_pages);
    RUN_TEST(test_upper_banks_are_addressed_correctly);
    RUN_TEST(test_flush_address_only_flushes_that_page);
    RUN_TEST(test_dirty_page_flushed_after_full_aging_period);
    RUN_TEST(test_age_fully_flushes_on_next_tick);
    RUN_TEST(test_one_aged_page_flushed_per_tick);
    RUN_TEST(test_age_fully_on_uncached_address_is_harmless);
    RUN_TEST(test_clean_pages_never_written_by_aging);
    RUN_TEST(test_invalidate_writes_dirty_page_then_rereads);
    RUN_TEST(test_cached_page_hides_external_changes_until_invalidated);
    RUN_TEST(test_invalidate_all_persists_every_dirty_page);
    RUN_TEST(test_invalidate_page_out_of_range_is_ignored);
    RUN_TEST(test_evicts_clean_pages_without_writing);
    RUN_TEST(test_cache_full_of_dirty_pages_evicts_by_flushing);
    RUN_TEST(test_data_survives_a_new_cache_instance);
    RUN_TEST(test_mutex_not_reentered);
    RUN_TEST(test_read_failure_is_reported);
    RUN_TEST(test_write_failure_keeps_page_dirty);
    return UNITY_END();
}
