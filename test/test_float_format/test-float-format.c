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

void setUp(void) {
    // Here any initialization code can be placed that needs to run before each test
}

void tearDown(void) {
    // Here any cleanup code can be placed that needs to run after each test
}

void testFloatFormat(void) {
    // Example test case
    char buffer[10];
    int result = float_to_string(12.34, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(-1, result);
    TEST_ASSERT_EQUAL_STRING("12.3", buffer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testFloatFormat);
    return UNITY_END();
}