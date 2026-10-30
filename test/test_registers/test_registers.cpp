#include <unity.h>
#include <DeyeRegisters.h>

void setUp() {}
void tearDown() {}

void test_writable_whitelist()
{
    // evcc battery-control and curtailment registers must pass
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(109));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(127));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(128));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(130));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(143));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(146));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(166));
    TEST_ASSERT_TRUE(DeyeRegisters::IsWritable(171));

    // Measurement and unknown registers must be rejected
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(625));
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(588));
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(100));
    TEST_ASSERT_FALSE(DeyeRegisters::IsWritable(0));
}

void test_poll_blocks_cover_evcc_registers()
{
    bool covered[800] = {};

    for (size_t b = 0; b < DeyeRegisters::kPollBlockCount; ++b)
    {
        const auto &block = DeyeRegisters::kPollBlocks[b];
        for (uint16_t r = 0; r < block.count; ++r)
        {
            covered[block.start + r] = true;
        }
    }

    // Registers used by the evcc deye-hybrid-3p template
    const uint16_t required[] = {
        516, 518, 520, 521, 522, 524, 526, 527, 529, 534,
        588, 589, 590, 591, 595,
        598, 599, 600, 610, 611, 612, 613, 614, 615,
        625, 636, 638,
        650, 651, 652, 653,
        667, 671, 672, 673, 674, 675,
        143, 109, 127, 128, 130, 146,
        166, 167, 168, 169, 170, 171,
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
    RUN_TEST(test_poll_blocks_cover_evcc_registers);
    return UNITY_END();
}
