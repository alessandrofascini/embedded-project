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

static const struct NtcParams ntc_test_params = {
    .r_pullup = 10000.0f, // 10k ohm pull-up resistor
    .r0_ohms = 10000.0f,  // 10k ohm nominal resistance at t0_kelvin
    .t0_kelvin = 298.15f, // 25 degC in kelvin
    .beta = 3950.0f,      // Beta coefficient
};

void testNtcInit(void) {
    struct Ntc ntc;

    enum NtcError result = ntc_init(&ntc, &ntc_test_params);
    TEST_ASSERT_EQUAL(NTC_OK, result);

    // Test invalid parameters: each case breaks one field of otherwise valid params
    TEST_ASSERT_EQUAL(NTC_SELF_IS_NULL, ntc_init(NULL, &ntc_test_params));
    TEST_ASSERT_EQUAL(NTC_PARAMS_IS_NULL, ntc_init(&ntc, NULL));

    struct NtcParams params = ntc_test_params;
    params.r_pullup = 0.0f;
    TEST_ASSERT_EQUAL(NTC_INVALID_R_PULLUP, ntc_init(&ntc, &params));

    params = ntc_test_params;
    params.r0_ohms = 0.0f;
    TEST_ASSERT_EQUAL(NTC_INVALID_R0, ntc_init(&ntc, &params));

    params = ntc_test_params;
    params.t0_kelvin = 0.0f;
    TEST_ASSERT_EQUAL(NTC_INVALID_T0, ntc_init(&ntc, &params));

    params = ntc_test_params;
    params.beta = 0.0f;
    TEST_ASSERT_EQUAL(NTC_INVALID_BETA, ntc_init(&ntc, &params));
}

void testNtcRawAdcToCelsius(void) {
    struct Ntc ntc;

    TEST_ASSERT_EQUAL_FLOAT(NAN, ntc_raw_adc_to_celsius(NULL, 0));
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, ntc_raw_adc_to_celsius(&ntc, 0));
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, ntc_raw_adc_to_celsius(&ntc, NTC_ADC_MAX_VALUE));

    TEST_ASSERT_EQUAL(NTC_OK, ntc_init(&ntc, &ntc_test_params));

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, ntc_raw_adc_to_celsius(&ntc, 2048)); // Mid-scale ADC value

    /* Away from T0 the logarithmic term matters: expected values from the Beta model */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.18f, ntc_raw_adc_to_celsius(&ntc, 1024));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 51.99f, ntc_raw_adc_to_celsius(&ntc, 3072));

    /* With r_pullup != r0 (the board's NTC), the two resistances cannot cancel out */
    struct NtcParams board_params = ntc_test_params;
    board_params.r_pullup = 18000.0f;
    board_params.r0_ohms = 8755.0f;
    TEST_ASSERT_EQUAL(NTC_OK, ntc_init(&ntc, &board_params));
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
