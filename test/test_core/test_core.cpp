#include <unity.h>
#include <cstring>
#include <cstdio>
#include <core/WriteQueue.h>
#include <core/RegisterCache.h>
#include <core/ValueFormat.h>
#include <Config.h>

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

void test_state_json_fits_mqtt_buffer()
{
    // Regression: the JSON state message must stay publishable on MQTT.
    // PubSubClient rejects publish() when header + topic + payload exceed its
    // buffer; with the old 768-byte buffer the {base}/state topic silently
    // never carried data.
    static RegisterCache cache(Config::RegisterSpaceEnd);

    static uint16_t worst[Config::RegisterSpaceEnd];
    for (size_t i = 0; i < Config::RegisterSpaceEnd; ++i)
    {
        worst[i] = 0xFFFF;
    }
    cache.Store(0, worst, Config::RegisterSpaceEnd);

    // Per-entity worst cases on top of the all-ones baseline
    for (size_t i = 0; i < DeyeRegisters::kSensorCount; ++i)
    {
        const DeyeRegisters::SensorEntity &e = DeyeRegisters::kSensors[i];
        if (e.isDword)
        {
            cache.Poke(e.address, 0xFFFF);
            cache.Poke(e.address + 1, 0x7FFF);   // +214748364.7, longest value
        }
        else if (e.isSigned)
        {
            cache.Poke(e.address, 0x8000);       // -32768
        }
    }

    static char json[4096];
    BuildStateJson(cache, json, sizeof(json));
    const size_t length = strlen(json);
    printf("      worst-case state JSON: %u bytes\n", static_cast<unsigned>(length));

    TEST_ASSERT_TRUE(length > 0);
    TEST_ASSERT_EQUAL('}', json[length - 1]);   // complete object, no truncation

    // MQTT publish overhead: header (5) + topic length field (2) + topic
    // (mBase up to 39 chars + "state"); keep 64 bytes of topic headroom
    const size_t maxPayload = Config::MqttBufferBytes - 5 - 2 - 64;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(static_cast<uint32_t>(maxPayload),
                                     static_cast<uint32_t>(length));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_write_queue_fifo_and_mask);
    RUN_TEST(test_register_cache_store_get_valid);
    RUN_TEST(test_state_json_fits_mqtt_buffer);
    return UNITY_END();
}
