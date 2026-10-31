#include <unity.h>
#include <util/StrAppend.h>
#include <util/HtmlEscape.h>
#include <cstring>

void setUp() {}
void tearDown() {}

void test_str_append_basic_and_truncation()
{
    char buf[16];
    size_t n = 0;
    Util::StrAppend(buf, sizeof(buf), n, "{");
    Util::StrAppend(buf, sizeof(buf), n, "\"k\":%d,", 42);
    Util::StrAppend(buf, sizeof(buf), n, "\"long\":\"%s\"", "0123456789ABCDEFGH");
    Util::StrAppend(buf, sizeof(buf), n, "}");
    TEST_ASSERT_EQUAL_STRING("{\"k\":42,\"long\":", buf);
    TEST_ASSERT_EQUAL_UINT(15, n);
}

void test_str_append_stops_when_full()
{
    char buf[8] = {0};
    size_t n = 0;
    Util::StrAppend(buf, sizeof(buf), n, "12345678901234567890");
    Util::StrAppend(buf, sizeof(buf), n, "more");
    TEST_ASSERT_EQUAL_STRING("1234567", buf);
    TEST_ASSERT_EQUAL_UINT(7, n);
}

void test_html_escape_entities()
{
    char out[64];
    Util::HtmlEscape("<a href='x'>&\"", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("&lt;a href=&#39;x&#39;&gt;&amp;&quot;", out);
    Util::HtmlEscape("plain", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("plain", out);
    Util::HtmlEscape("", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_html_escape_truncates_safely()
{
    char small[8];
    Util::HtmlEscape("a&b&c&d", small, sizeof(small));
    TEST_ASSERT_EQUAL_STRING("a&amp;b", small);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_str_append_basic_and_truncation);
    RUN_TEST(test_str_append_stops_when_full);
    RUN_TEST(test_html_escape_entities);
    RUN_TEST(test_html_escape_truncates_safely);
    return UNITY_END();
}
