#include <unity.h>
#include <DeyeRegisters.h>

void setUp() {}
void tearDown() {}

void test_writable_whitelist()
{
    // evcc battery-control and curtailment registers must pass
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(80));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(104));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(108));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(109));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(127));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(128));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(130));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(141));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(142));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(143));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(145));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(146));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(154));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(160));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(166));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(171));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(172));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(177));

    // Measurement and unknown registers must be rejected
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(625));
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(588));
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(148));
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(552));
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(0));
}

void test_poll_blocks_cover_evcc_and_esphome_registers()
{
    bool covered[800] = {};

    for (size_t b = 0; b < DeyeRegisters::kPollBlockCount; ++b)
    {
        const auto &block = DeyeRegisters::kPollBlocks[b];
        for (uint16_t r = 0; r < block.count; ++r)
        {
            TEST_ASSERT_TRUE_MESSAGE(block.start + r < 800, "block exceeds test window");
            covered[block.start + r] = true;
        }
    }

    // Registers used by the evcc deye-hybrid-3p template and the reference
    // ESPHome configuration
    const uint16_t required[] = {
        80, 104, 108, 109, 115, 127, 128, 130,
        141, 142, 143, 145, 146,
        148, 149, 150, 151, 152, 153,
        154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165,
        166, 167, 168, 169, 170, 171,
        172, 173, 174, 175, 176, 177,
        500,
        514, 515, 516, 518, 520, 521, 522, 524, 526, 527, 529, 534,
        540, 541,
        552,
        586, 587, 588, 589, 590, 591, 595,
        598, 599, 600, 609, 610, 611, 612, 613, 614, 615,
        622, 623, 624, 625,
        627, 628, 629, 633, 634, 635, 636, 638,
        650, 651, 652, 653,
        664, 665, 666, 667, 671, 672, 673, 674, 675,
        676, 677, 678, 679, 680, 681, 682, 683,
    };
    for (const uint16_t reg : required)
    {
        TEST_ASSERT_TRUE_MESSAGE(covered[reg], "register not polled");
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_writable_whitelist);
    RUN_TEST(test_poll_blocks_cover_evcc_and_esphome_registers);
    return UNITY_END();
}
