/*!
 * \file test-float-format.c
 * \author Alessandro Fascini
 *
 * \brief Float format test file.
 */

#include "unity.h"
#include "float-format-api.h"
// Include the FFF library for function mocking (must be pasted into test/include/fff.h)
#include "fff.h"

struct FloatFormatCase {
    float value;
    char *buffer;
    uint16_t buffer_size;
    enum FloatFormatError expected_result;
    const char *expected_string; // NULL if the conversion is expected to fail (buffer content undefined/irrelevant)
};

void setUp(void) {
    // Here any initialization code can be placed that needs to run before each test
}

void tearDown(void) {
    // Here any cleanup code can be placed that needs to run after each test
}

void testFloatFormatCases(void) {
    char buffer[16];

    const struct FloatFormatCase test_cases[] = {
        { NAN, buffer, 10, FLOAT_FORMAT_ERR_NAN, NULL },
        { .0f, NULL, 0, FLOAT_FORMAT_ERR_NULL, NULL },
        { .0f, buffer, 0, FLOAT_FORMAT_ERR_NULL, NULL },
        { .0f, buffer, 1, FLOAT_FORMAT_ERR_BUFFER_TOO_SMALL, NULL },
        { .0f, buffer, 2, FLOAT_FORMAT_ERR_BUFFER_TOO_SMALL, NULL },
        { 0.0f, buffer, 3, FLOAT_FORMAT_ERR_OVERFLOW, NULL },
        { 123.4f, buffer, 4, FLOAT_FORMAT_ERR_OVERFLOW, NULL },
        { -1.2f, buffer, 4, FLOAT_FORMAT_ERR_OVERFLOW, NULL },
        { 12.34f, buffer, 16, FLOAT_FORMAT_OK, "12.3" },
        { -12.34f, buffer, 16, FLOAT_FORMAT_OK, "-12.3" },
        { 0.0f, buffer, 16, FLOAT_FORMAT_OK, "0.0" },
        { -0.0f, buffer, 16, FLOAT_FORMAT_OK, "0.0" },
    };
    const size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    for (size_t i = 0; i < num_cases; i++) {
        const struct FloatFormatCase *c = &test_cases[i];
        int result = float_to_string(c->value, buffer, c->buffer_size);
        TEST_ASSERT_EQUAL_INT(c->expected_result, result);
        if (c->expected_string != NULL) {
            TEST_ASSERT_EQUAL_STRING(c->expected_string, buffer);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testFloatFormatCases);
    return UNITY_END();
}