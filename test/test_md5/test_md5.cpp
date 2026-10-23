/*
 * MD5 implementation used to verify ESP32 firmware images before flashing.
 * Vectors are from RFC 1321 plus lengths straddling the 64 byte block and
 * 56 byte padding boundaries.
 */
#include "gevcu_test.h"
#include "devices/esp32/md5_hash.h"

static void md5Hex(const uint8_t *data, size_t len, char out[33], size_t chunk = 0)
{
    MD5Context ctx;
    unsigned char digest[16];
    MD5Init(&ctx);
    if (chunk == 0) chunk = len ? len : 1;
    for (size_t off = 0; off < len; off += chunk)
    {
        size_t n = (len - off < chunk) ? len - off : chunk;
        MD5Update(&ctx, data + off, (unsigned)n);
    }
    MD5Final(digest, &ctx);
    for (int i = 0; i < 16; i++) sprintf(out + 2 * i, "%02x", digest[i]);
}

static void assertMd5Str(const char *input, const char *expected)
{
    char hex[33];
    md5Hex((const uint8_t *)input, strlen(input), hex);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, hex, input);
}

void setUp() { hostResetAll(); }
void tearDown() {}

void test_rfc1321_vectors()
{
    assertMd5Str("", "d41d8cd98f00b204e9800998ecf8427e");
    assertMd5Str("a", "0cc175b9c0f1b6a831c399e269772661");
    assertMd5Str("abc", "900150983cd24fb0d6963f7d28e17f72");
    assertMd5Str("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
    assertMd5Str("abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b");
    assertMd5Str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                 "d174ab98d277d9f5a5611c2c9f419d9f");
    assertMd5Str("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                 "57edf4a22be3c955ac49da2e2107b67a");
}

void test_block_boundary_lengths()
{
    struct { size_t len; const char *md5; } cases[] = {
        {55, "6912ee65fff2d9f9ce2508cddf8bcda0"},
        {56, "51fdd1acda72405dfdfa03fcb85896d7"},
        {63, "48a6295221902e8e0938f773a7185e72"},
        {64, "b2d3f56bc197fd985d5965079b5e7148"},
        {65, "8bd7053801c768420faf816fadba971c"},
        {1000, "cbecbdb0fdd5cec1e242493b6008cc79"},
    };
    uint8_t buf[1000];
    for (int i = 0; i < 1000; i++) buf[i] = (uint8_t)i;
    for (auto &c : cases)
    {
        char hex[33];
        md5Hex(buf, c.len, hex);
        TEST_ASSERT_EQUAL_STRING(c.md5, hex);
    }
}

void test_chunked_updates_match_single_update()
{
    static uint8_t buf[100000];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)(i * 7);
    const size_t chunks[] = {1, 3, 63, 64, 65, 1024, 4096, sizeof(buf)};
    for (size_t chunk : chunks)
    {
        char hex[33];
        md5Hex(buf, sizeof(buf), hex, chunk);
        TEST_ASSERT_EQUAL_STRING("8cb7f568752be648376cd32a342006af", hex);
    }
}

void test_zero_length_update_is_harmless()
{
    MD5Context ctx;
    unsigned char digest[16];
    char hex[33];
    MD5Init(&ctx);
    MD5Update(&ctx, (const unsigned char *)"ab", 2);
    MD5Update(&ctx, (const unsigned char *)"", 0);
    MD5Update(&ctx, (const unsigned char *)"c", 1);
    MD5Final(digest, &ctx);
    for (int i = 0; i < 16; i++) sprintf(hex + 2 * i, "%02x", digest[i]);
    TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", hex);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_rfc1321_vectors);
    RUN_TEST(test_block_boundary_lengths);
    RUN_TEST(test_chunked_updates_match_single_update);
    RUN_TEST(test_zero_length_update_is_harmless);
    return UNITY_END();
}
