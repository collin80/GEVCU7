/*
 * Crash breadcrumb encoding. ENCODE_BREAD packs five upper-case letters into
 * the top 25 bits of a 32 bit crumb, leaving a 7 bit counter;
 * decodeBreadcrumbToString() must reverse that so post-crash reports name the
 * right function.
 */
#include "gevcu_test.h"
#include "CrashHandler.h"

void setUp() { hostResetAll(); }
void tearDown() {}

void test_encode_layout()
{
    uint32_t v = ENCODE_BREAD("ABCDE");
    TEST_ASSERT_EQUAL_HEX32((1u << 27) | (2u << 22) | (3u << 17) | (4u << 12) | (5u << 7), v);
    TEST_ASSERT_EQUAL_HEX32(0, v & 0x7F); // counter bits left clear
}

void test_decode_round_trips_name()
{
    const char *names[] = {"ABCDE", "MCACH", "PREFH", "ZZZZZ", "SETUP", "CANHL"};
    for (const char *n : names)
    {
        char buf[16];
        CrashHandler ch;
        ch.decodeBreadcrumbToString(ENCODE_BREAD(n), buf);
        TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(n, buf, 5, n);
    }
}

void test_decode_appends_counter_as_hex()
{
    char buf[16];
    CrashHandler ch;
    ch.decodeBreadcrumbToString(ENCODE_BREAD("TICKH") + 0x2A, buf);
    TEST_ASSERT_EQUAL_STRING("TICKH2a", buf);
    ch.decodeBreadcrumbToString(ENCODE_BREAD("TICKH") + 0x7F, buf);
    TEST_ASSERT_EQUAL_STRING("TICKH7f", buf);
    ch.decodeBreadcrumbToString(ENCODE_BREAD("TICKH") + 3, buf);
    TEST_ASSERT_EQUAL_STRING("TICKH03", buf);
}

void test_counter_never_bleeds_into_name()
{
    char buf[16];
    CrashHandler ch;
    for (uint32_t c = 0; c < 128; c++)
    {
        ch.decodeBreadcrumbToString(ENCODE_BREAD("MOTOR") + c, buf);
        TEST_ASSERT_EQUAL_STRING_LEN("MOTOR", buf, 5);
    }
}

void test_fresh_handler_reports_no_crash()
{
    CrashHandler ch;
    TEST_ASSERT_FALSE(ch.bCrashed());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_layout);
    RUN_TEST(test_decode_round_trips_name);
    RUN_TEST(test_decode_appends_counter_as_hex);
    RUN_TEST(test_counter_never_bleeds_into_name);
    RUN_TEST(test_fresh_handler_reports_no_crash);
    return UNITY_END();
}
