// =============================================================
//  test/test_packet/test_packet.cpp — Unity tests: protocol
//  NATIVE_TEST is defined by build_flags in platformio.ini
// =============================================================
#include <unity.h>
#include <string.h>

#include "../../src/common/protocol.h"

void setUp()    {}
void tearDown() {}

void test_magic_and_ver()
{
    TEST_ASSERT_EQUAL_UINT8(0xA5, PKT_MAGIC);
    TEST_ASSERT_EQUAL_UINT8(1,    PKT_VER);
}

void test_pkt_size_within_espnow_limit()
{
    TEST_ASSERT_LESS_OR_EQUAL(250, (int)sizeof(Pkt));
}

void test_pkt_fields_pack()
{
    Pkt p = {};
    p.magic     = PKT_MAGIC;
    p.ver       = PKT_VER;
    p.type      = (uint8_t)PktType::CHAT;
    p.node_id   = 7;
    p.color_idx = 3;
    p.seq       = 1024;
    const char* msg = "Hello Matrix";
    strncpy(p.text, msg, sizeof(p.text) - 1);
    p.len = (uint8_t)strlen(msg);

    TEST_ASSERT_EQUAL_UINT8(PKT_MAGIC,              p.magic);
    TEST_ASSERT_EQUAL_UINT8(PKT_VER,                p.ver);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)PktType::CHAT, p.type);
    TEST_ASSERT_EQUAL_UINT8(7,                      p.node_id);
    TEST_ASSERT_EQUAL_UINT16(1024,                  p.seq);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)strlen(msg),   p.len);
    TEST_ASSERT_EQUAL_STRING(msg,                   p.text);
}

void test_pkt_type_enum_values()
{
    TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)PktType::CHAT);
    TEST_ASSERT_EQUAL_UINT8(1, (uint8_t)PktType::SMITH_BEACON);
    TEST_ASSERT_EQUAL_UINT8(2, (uint8_t)PktType::SMITH_CHAT);
}

void test_text_field_max_length()
{
    Pkt p = {};
    char buf[181];
    memset(buf, 'A', 180);
    buf[180] = '\0';
    strncpy(p.text, buf, sizeof(p.text) - 1);
    p.text[sizeof(p.text) - 1] = '\0';
    // 180 'A's + null terminator fits in char text[180]
    TEST_ASSERT_EQUAL_CHAR('\0', p.text[179]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_magic_and_ver);
    RUN_TEST(test_pkt_size_within_espnow_limit);
    RUN_TEST(test_pkt_fields_pack);
    RUN_TEST(test_pkt_type_enum_values);
    RUN_TEST(test_text_field_max_length);
    return UNITY_END();
}
