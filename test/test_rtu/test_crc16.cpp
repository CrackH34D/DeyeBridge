#include <unity.h>
#include <rtu/Crc16.h>

void setUp() {}
void tearDown() {}

void test_crc16_known_vector()
{
    // Widely used Modbus example: FC03 request for 10 registers at address 0
    const uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    TEST_ASSERT_EQUAL_HEX16(0xCDC5, Rtu::Crc16(frame, sizeof(frame)));
}

void test_crc16_changes_with_data()
{
    const uint8_t a[] = {0x01, 0x06, 0x00, 0x8F, 0x00, 0x64};
    const uint8_t b[] = {0x01, 0x06, 0x00, 0x8F, 0x00, 0x65};
    TEST_ASSERT_NOT_EQUAL(Rtu::Crc16(a, sizeof(a)), Rtu::Crc16(b, sizeof(b)));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc16_known_vector);
    RUN_TEST(test_crc16_changes_with_data);
    return UNITY_END();
}
