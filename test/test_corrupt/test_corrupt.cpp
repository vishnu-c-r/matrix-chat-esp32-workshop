// =============================================================
//  test/test_corrupt/test_corrupt.cpp — Unity tests: corruption
// =============================================================
#include <unity.h>
#include <string.h>

#include "../../src/smith/corrupt.cpp"

void setUp()    {}
void tearDown() {}

void test_same_seed_produces_same_output()
{
    char a[64] = "The Matrix has you...";
    char b[64] = "The Matrix has you...";
    corrupt(a, 3, 42u);
    corrupt(b, 3, 42u);
    TEST_ASSERT_EQUAL_STRING(a, b);
}

void test_different_seeds_produce_different_output()
{
    char a[64] = "Free your mind.";
    char b[64] = "Free your mind.";
    corrupt(a, 3, 1u);
    corrupt(b, 3, 999u);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(a, b));
}

void test_null_terminator_preserved()
{
    char text[32] = "Mr. Anderson";
    size_t original_len = strlen(text);
    corrupt(text, 5, 777u);
    TEST_ASSERT_LESS_OR_EQUAL(original_len, strlen(text));
    TEST_ASSERT_EQUAL_CHAR('\0', text[original_len]);
}

void test_empty_string_is_unchanged()
{
    char text[8] = "";
    corrupt(text, 5, 123u);
    TEST_ASSERT_EQUAL_STRING("", text);
}

void test_level_1_changes_fewer_chars_than_level_5()
{
    const char* base = "There is no spoon in the Matrix world.";
    char a[64], b[64];
    strncpy(a, base, sizeof(a) - 1); a[sizeof(a) - 1] = '\0';
    strncpy(b, base, sizeof(b) - 1); b[sizeof(b) - 1] = '\0';
    corrupt(a, 1, 42u);
    corrupt(b, 5, 42u);
    int diff_a = 0, diff_b = 0;
    for (size_t i = 0; i < strlen(base); ++i) {
        if (a[i] != base[i]) ++diff_a;
        if (b[i] != base[i]) ++diff_b;
    }
    TEST_ASSERT_LESS_THAN(diff_b, diff_a + 1);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_same_seed_produces_same_output);
    RUN_TEST(test_different_seeds_produce_different_output);
    RUN_TEST(test_null_terminator_preserved);
    RUN_TEST(test_empty_string_is_unchanged);
    RUN_TEST(test_level_1_changes_fewer_chars_than_level_5);
    return UNITY_END();
}
