/*
 * Intel HEX parsing used by the SD card / serial firmware updater (FlasherX).
 * A corrupt record accepted here would be written into program flash, so
 * the rejection paths matter as much as the happy path.
 */
#include "gevcu_test.h"
#include "intel_hex.h"

extern Stream *serialptr;

static char bytes[256];
static unsigned int addr, num, code;

void setUp()
{
    hostResetAll();
    memset(bytes, 0, sizeof(bytes));
    addr = num = code = 0xDEADBEEF;
}
void tearDown() {}

// ------------------------------------------------------------ parse_hex_line

void test_parses_data_record()
{
    TEST_ASSERT_EQUAL_INT(1, parse_hex_line(":109D3000711F0000AD38000005390000F546000035", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_UINT(0x9D30, addr);
    TEST_ASSERT_EQUAL_UINT(16, num);
    TEST_ASSERT_EQUAL_UINT(0, code);
    const uint8_t expected[16] = {0x71, 0x1F, 0x00, 0x00, 0xAD, 0x38, 0x00, 0x00,
                                  0x05, 0x39, 0x00, 0x00, 0xF5, 0x46, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, bytes, 16);
}

void test_parses_short_data_record()
{
    TEST_ASSERT_EQUAL_INT(1, parse_hex_line(":049D400001480000D6", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_UINT(0x9D40, addr);
    TEST_ASSERT_EQUAL_UINT(4, num);
    TEST_ASSERT_EQUAL_UINT8(0x01, (uint8_t)bytes[0]);
    TEST_ASSERT_EQUAL_UINT8(0x48, (uint8_t)bytes[1]);
}

void test_parses_eof_record()
{
    TEST_ASSERT_EQUAL_INT(1, parse_hex_line(":00000001FF", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_UINT(1, code);
    TEST_ASSERT_EQUAL_UINT(0, num);
}

void test_accepts_lowercase_hex_and_trailing_newline()
{
    TEST_ASSERT_EQUAL_INT(1, parse_hex_line(":049d400001480000d6\r\n", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_UINT(0x9D40, addr);
}

void test_rejects_bad_checksum()
{
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":049D400001480000D7", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":109D3000711F0000AD38000005390000F546000036", bytes, &addr, &num, &code));
}

void test_rejects_flipped_data_bit()
{
    // Same record as above with one data nibble changed but original checksum.
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":049D400001490000D6", bytes, &addr, &num, &code));
}

void test_rejects_missing_start_code()
{
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line("049D400001480000D6", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(";049D400001480000D6", bytes, &addr, &num, &code));
}

void test_rejects_too_short_line()
{
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line("", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":00000001F", bytes, &addr, &num, &code));
}

void test_rejects_line_shorter_than_declared_length()
{
    // Declares 16 data bytes but only carries 4.
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":109D400001480000D6", bytes, &addr, &num, &code));
}

void test_rejects_non_hex_payload()
{
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":04ZZ400001480000D6", bytes, &addr, &num, &code));
    TEST_ASSERT_EQUAL_INT(0, parse_hex_line(":049D4000XX480000D6", bytes, &addr, &num, &code));
}

// -------------------------------------------------------- process_hex_record

static hex_info_t freshInfo(char *data)
{
    hex_info_t h;
    memset(&h, 0, sizeof(h));
    h.data = data;
    h.min = 0xFFFFFFFF;
    h.max = 0;
    return h;
}

void test_data_records_track_min_and_max()
{
    char data[16] = {0};
    hex_info_t h = freshInfo(data);
    h.base = 0x60000000;

    h.code = 0; h.addr = 0x1000; h.num = 16;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
    h.code = 0; h.addr = 0x0100; h.num = 4;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
    h.code = 0; h.addr = 0x2000; h.num = 8;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));

    TEST_ASSERT_EQUAL_HEX32(0x60000100, h.min);
    TEST_ASSERT_EQUAL_HEX32(0x60002008, h.max); // one past the last byte
}

void test_eof_record_sets_flag()
{
    char data[1] = {0};
    hex_info_t h = freshInfo(data);
    h.code = 1;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
    TEST_ASSERT_EQUAL_INT(1, h.eof);
}

void test_extended_segment_address()
{
    char data[2] = {0x12, 0x34};
    hex_info_t h = freshInfo(data);
    h.code = 2;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
    TEST_ASSERT_EQUAL_HEX32(0x12340, h.base);
}

void test_extended_linear_address()
{
    char data[2] = {0x60, 0x00};
    hex_info_t h = freshInfo(data);
    h.code = 4;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
    TEST_ASSERT_EQUAL_HEX32(0x60000000, h.base);
}

void test_extended_linear_address_high_bit()
{
    // Needs -funsigned-char (as on ARM) or 0x80 would sign extend.
    char data[2] = {(char)0x80, (char)0xFF};
    hex_info_t h = freshInfo(data);
    h.code = 4;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
    TEST_ASSERT_EQUAL_HEX32(0x80FF0000, h.base);
}

void test_start_linear_address()
{
    char data[4] = {0x60, 0x00, 0x20, (char)0xD1};
    hex_info_t h = freshInfo(data);
    h.code = 5;
    TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
    TEST_ASSERT_EQUAL_HEX32(0x600020D1, h.base);
}

void test_start_segment_and_unknown_records_are_errors()
{
    char data[4] = {0};
    hex_info_t h = freshInfo(data);
    h.code = 3;
    TEST_ASSERT_EQUAL_INT(1, process_hex_record(&h));
    h.code = 6;
    TEST_ASSERT_EQUAL_INT(1, process_hex_record(&h));
    h.code = 0xFF;
    TEST_ASSERT_EQUAL_INT(1, process_hex_record(&h));
}

void test_full_file_parse_through_fsfile()
{
    FsFile file(
        ":020000040060" "9A\n"
        ":10000000000102030405060708090A0B0C0D0E0F" "78\n"
        ":10001000101112131415161718191A1B1C1D1E1F" "68\n"
        ":00000001FF\n");
    char line[96];
    char data[256];
    hex_info_t h = freshInfo(data);
    int records = 0;
    while (!h.eof)
    {
        read_ascii_line_file(&file, line, sizeof(line));
        TEST_ASSERT_TRUE_MESSAGE(strlen(line) > 0, "ran out of lines before EOF record");
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, parse_hex_line(line, h.data, &h.addr, &h.num, &h.code), line);
        TEST_ASSERT_EQUAL_INT(0, process_hex_record(&h));
        records++;
    }
    TEST_ASSERT_EQUAL_INT(4, records);
    TEST_ASSERT_EQUAL_HEX32(0x00600000, h.min);
    TEST_ASSERT_EQUAL_HEX32(0x00600020, h.max);
}

void test_read_ascii_line_serial_terminates_line()
{
    serialptr = &Serial2;
    Serial2.hostFeed(":00000001FF\n:049D400001480000D6\r");
    char line[64];
    read_ascii_line_serial(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING(":00000001FF", line);
    read_ascii_line_serial(line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING(":049D400001480000D6", line);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_parses_data_record);
    RUN_TEST(test_parses_short_data_record);
    RUN_TEST(test_parses_eof_record);
    RUN_TEST(test_accepts_lowercase_hex_and_trailing_newline);
    RUN_TEST(test_rejects_bad_checksum);
    RUN_TEST(test_rejects_flipped_data_bit);
    RUN_TEST(test_rejects_missing_start_code);
    RUN_TEST(test_rejects_too_short_line);
    RUN_TEST(test_rejects_line_shorter_than_declared_length);
    RUN_TEST(test_rejects_non_hex_payload);
    RUN_TEST(test_data_records_track_min_and_max);
    RUN_TEST(test_eof_record_sets_flag);
    RUN_TEST(test_extended_segment_address);
    RUN_TEST(test_extended_linear_address);
    RUN_TEST(test_extended_linear_address_high_bit);
    RUN_TEST(test_start_linear_address);
    RUN_TEST(test_start_segment_and_unknown_records_are_errors);
    RUN_TEST(test_full_file_parse_through_fsfile);
    RUN_TEST(test_read_ascii_line_serial_terminates_line);
    return UNITY_END();
}
