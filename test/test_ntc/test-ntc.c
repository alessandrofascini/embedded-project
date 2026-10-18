/*!
 * \file test-ntc.c
 * \author Alessandro Fascini
 *
 * \brief NTC test file.
 */

#include "unity.h"
#include "ntc-api.h"
// Include the FFF library for function mocking (must be pasted into test/include/fff.h)
#include "fff.h"

void setUp(void) {
    // Here any initialization code can be placed that needs to run before each test
}

void tearDown(void) {
    // Here any cleanup code can be placed that needs to run after each test
}

void testNtcInit(void) {
    struct Ntc ntc;
    const float r_pullup = 10000.0f; // 10k ohm pull-up resistor
    const float r0 = 10000.0f;       // 10k ohm nominal resistance at t0_kelvin
    const float t0_kelvin = 298.15f; // 25 degC in kelvin
    const float beta = 3950.0f;      // Beta coefficient

    enum NtcError result = ntc_init(&ntc, r_pullup, r0, t0_kelvin, beta);
    TEST_ASSERT_EQUAL(NTC_OK, result);

    // Test invalid parameters
    TEST_ASSERT_EQUAL(NTC_SELF_IS_NULL, ntc_init(NULL, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_R_PULLUP, ntc_init(&ntc, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_R0, ntc_init(&ntc, r_pullup, 0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_T0, ntc_init(&ntc, r_pullup, r0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_BETA, ntc_init(&ntc, r_pullup, r0, t0_kelvin, 0));
}

void testNtcRawAdcToCelsius(void) {
    struct Ntc ntc;

    TEST_ASSERT_EQUAL_FLOAT(NAN, ntc_raw_adc_to_celsius(NULL, 0));
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, ntc_raw_adc_to_celsius(&ntc, 0));
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, ntc_raw_adc_to_celsius(&ntc, NTC_ADC_MAX_VALUE));

    const float r_pullup = 10000.0f; // 10k ohm pull-up resistor
    const float r0 = 10000.0f;       // 10k ohm nominal resistance at t0_kelvin
    const float t0_kelvin = 298.15f; // 25 degC in kelvin
    const float beta = 3950.0f;      // Beta coefficient

    TEST_ASSERT_EQUAL(NTC_OK, ntc_init(&ntc, r_pullup, r0, t0_kelvin, beta));

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, ntc_raw_adc_to_celsius(&ntc, 2048)); // Mid-scale ADC value

    /* Away from T0 the logarithmic term matters: expected values from the Beta model */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.18f, ntc_raw_adc_to_celsius(&ntc, 1024));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 51.99f, ntc_raw_adc_to_celsius(&ntc, 3072));

    /* With r_pullup != r0 (the board's NTC), the two resistances cannot cancel out */
    TEST_ASSERT_EQUAL(NTC_OK, ntc_init(&ntc, 18000.0f, 8755.0f, t0_kelvin, beta));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -11.00f, ntc_raw_adc_to_celsius(&ntc, 1024));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 9.63f, ntc_raw_adc_to_celsius(&ntc, 2048));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 33.78f, ntc_raw_adc_to_celsius(&ntc, 3072));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testNtcInit);
    RUN_TEST(testNtcRawAdcToCelsius);
    return UNITY_END();
}
