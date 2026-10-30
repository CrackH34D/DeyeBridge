#include <unity.h>
#include <core/WriteQueue.h>
#include <core/RegisterCache.h>

using DeyeBridge::RegisterCache;
using DeyeBridge::WriteEntry;
using DeyeBridge::WriteQueue;

void setUp() {}
void tearDown() {}

void test_write_queue_fifo_and_mask()
{
    WriteQueue queue;
    TEST_ASSERT_TRUE(queue.Empty());

    TEST_ASSERT_TRUE(queue.Push(109, 100, 0xFFFF));
    TEST_ASSERT_TRUE(queue.Push(145, 1, 0x0001));
    TEST_ASSERT_EQUAL(2, queue.Size());

    WriteEntry entry;
    TEST_ASSERT_TRUE(queue.Pop(entry));
    TEST_ASSERT_EQUAL(109, entry.address);
    TEST_ASSERT_EQUAL(100, entry.value);
    TEST_ASSERT_EQUAL(0xFFFF, entry.clearMask);
    TEST_ASSERT_TRUE(queue.Pop(entry));
    TEST_ASSERT_EQUAL(145, entry.address);
    TEST_ASSERT_EQUAL(1, entry.value);
    TEST_ASSERT_EQUAL(0x0001, entry.clearMask);
    TEST_ASSERT_TRUE(queue.Empty());
    TEST_ASSERT_FALSE(queue.Pop(entry));

    for (size_t i = 0; i < WriteQueue::kCapacity; ++i)
    {
        TEST_ASSERT_TRUE(queue.Push(static_cast<uint16_t>(i), 1, 0xFFFF));
    }
    TEST_ASSERT_FALSE(queue.Push(999, 1, 0xFFFF));
    TEST_ASSERT_EQUAL(WriteQueue::kCapacity, queue.Size());
}

void test_register_cache_store_get_valid()
{
    RegisterCache cache(100);
    const uint16_t values[] = {10, 20, 30};

    TEST_ASSERT_FALSE(cache.IsValid(5));
    cache.Store(5, values, 3);
    TEST_ASSERT_TRUE(cache.IsValid(5));
    TEST_ASSERT_TRUE(cache.IsValid(7));
    TEST_ASSERT_FALSE(cache.IsValid(8));
    TEST_ASSERT_EQUAL(10, cache.Get(5));
    TEST_ASSERT_EQUAL(30, cache.Get(7));
    TEST_ASSERT_EQUAL(0, cache.Get(99));   // outside stored block
    TEST_ASSERT_EQUAL(0, cache.Get(500));  // outside window

    cache.Poke(5, 77);
    TEST_ASSERT_EQUAL(77, cache.Get(5));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_write_queue_fifo_and_mask);
    RUN_TEST(test_register_cache_store_get_valid);
    return UNITY_END();
}
