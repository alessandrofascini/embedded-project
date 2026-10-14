/*!
 * \file test-adc-filter.c
 * \author Alessandro Fascini
 *
 * \brief AdcFilter test file.
 */
#include "unity.h"
#include "adc-filter-api.h"
// Include the FFF library for function mocking (must be pasted into test/include/fff.h)
#include "fff.h"

/* These mirror AdcFilter's internal noise amplitude/max-raw-value
 * constants, which are private to adc-filter-api.c and not part of
 * the public API. Duplicated here as the behavioral contract this
 * test verifies; update if the module's noise parameters change. */
#define TEST_NOISE_AMPLITUDE (20)
#define TEST_MAX_RAW_VALUE (4095)

void setUp(void) {
    // Here any initialization code can be placed that needs to run before each test
}

void tearDown(void) {
    // Here any cleanup code can be placed that needs to run after each test
}

void testAdcFilterInit(void) {
    TEST_ASSERT_EQUAL(ADC_FILTER_SELF_IS_NULL, adc_filter_init(NULL));

    struct AdcFilter filter;
    const enum AdcFilterError error = adc_filter_init(&filter);
    TEST_ASSERT_EQUAL(ADC_FILTER_OK, error);
    TEST_ASSERT_NOT_NULL(filter.apply);

    /*
     * ADC_FILTER_MOVING_AVERAGE_INIT_FAILED is left uncovered here:
     * adc_moving_average_init() can only fail on a NULL self/buffer or zero
     * capacity, and adc_filter_init() always passes a valid self (checked
     * above) and this struct's own fixed buffer/capacity, so the failure
     * branch is unreachable through the public API. Testing it would
     * require mocking adc_moving_average_init with FFF, which would need
     * excluding the real implementation from this suite's link and break
     * the other tests here that rely on the real moving average. */
}

void testAdcFilterSetModeAndGetMode(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);

    const enum AdcFilterMode modes[] = {
        ADC_FILTER_RAW,
        ADC_FILTER_NOISE,
        ADC_FILTER_MOVING_AVERAGE,
        ADC_FILTER_NOISE_AND_AVERAGE,
    };
    const size_t num_modes = sizeof(modes) / sizeof(modes[0]);

    for (size_t i = 0; i < num_modes; i++) {
        adc_filter_set_mode(&filter, modes[i]);
        TEST_ASSERT_EQUAL(modes[i], adc_filter_get_mode(&filter));
    }

    adc_filter_set_mode(NULL, ADC_FILTER_NOISE);
    TEST_ASSERT_EQUAL(ADC_FILTER_ERROR_MODE, adc_filter_get_mode(NULL));

    filter.apply = NULL; /* simulate a corrupted instance */
    TEST_ASSERT_EQUAL(ADC_FILTER_UNKNOWN_MODE, adc_filter_get_mode(&filter));

    /* Testing invalid modes */
    adc_filter_set_mode(&filter, ADC_FILTER_MOVING_AVERAGE);

    /* Passing an invalid mode must be a no-op: apply stays unchanged */
    adc_filter_set_mode(&filter, ADC_FILTER_ERROR_MODE);
    TEST_ASSERT_EQUAL(ADC_FILTER_MOVING_AVERAGE, adc_filter_get_mode(&filter));
    adc_filter_set_mode(&filter, ADC_FILTER_UNKNOWN_MODE);
    TEST_ASSERT_EQUAL(ADC_FILTER_MOVING_AVERAGE, adc_filter_get_mode(&filter));
}

void testAdcFilterApply(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);

    TEST_ASSERT_EQUAL_UINT16(2048, adc_filter_apply(&filter, 2048));

    adc_filter_set_mode(&filter, ADC_FILTER_MOVING_AVERAGE);
    adc_filter_apply(&filter, 1);
    adc_filter_apply(&filter, 2);
    const uint16_t result = adc_filter_apply(&filter, 3);
    TEST_ASSERT_EQUAL_UINT16(2, result); /* (1+2+3)/3 = 2.0 */

    TEST_ASSERT_EQUAL_UINT16(ADC_FILTER_INVALID_OPERATION, adc_filter_apply(NULL, 100));
}

void testAdcFilterSetModeResetsMovingAverage(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_set_mode(&filter, ADC_FILTER_MOVING_AVERAGE);

    adc_filter_apply(&filter, 100);
    adc_filter_apply(&filter, 200);
    const uint16_t before_switch = adc_filter_apply(&filter, 300);
    TEST_ASSERT_EQUAL_UINT16(200, before_switch); /* (100+200+300)/3 */

    adc_filter_set_mode(&filter, ADC_FILTER_RAW);
    adc_filter_set_mode(&filter, ADC_FILTER_MOVING_AVERAGE);

    /* window should be empty again, so the average of a single sample
     * equals that sample, not an average carried over from before */
    const uint16_t after_switch = adc_filter_apply(&filter, 300);
    TEST_ASSERT_EQUAL_UINT16(300, after_switch);
}

void testAdcFilterSetModeIsIdempotent(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_set_mode(&filter, ADC_FILTER_MOVING_AVERAGE);

    adc_filter_apply(&filter, 100);
    adc_filter_apply(&filter, 200);

    /* re-requesting the same mode must not reset the window */
    adc_filter_set_mode(&filter, ADC_FILTER_MOVING_AVERAGE);

    const uint16_t result = adc_filter_apply(&filter, 300);
    TEST_ASSERT_EQUAL_UINT16(200, result); /* (100+200+300)/3, not reset */
}

void testAdcFilterApplyNoise(void) {
    const uint16_t raw_value = 2048;

    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_set_mode(&filter, ADC_FILTER_NOISE);

    const uint16_t result = adc_filter_apply(&filter, raw_value);
    const int32_t lower_bound = (int32_t)raw_value - TEST_NOISE_AMPLITUDE;
    const int32_t upper_bound = (int32_t)raw_value + TEST_NOISE_AMPLITUDE;
    TEST_ASSERT_TRUE((int32_t)result >= lower_bound && (int32_t)result <= upper_bound);

    const uint16_t low = adc_filter_apply(&filter, 0);
    TEST_ASSERT_TRUE(low <= TEST_NOISE_AMPLITUDE); /* clamped at 0 */

    const uint16_t high = adc_filter_apply(&filter, TEST_MAX_RAW_VALUE);
    TEST_ASSERT_TRUE(high >= TEST_MAX_RAW_VALUE - TEST_NOISE_AMPLITUDE); /* clamped at 4095 */
}

void testAdcFilterApplyNoiseAndAverage(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_set_mode(&filter, ADC_FILTER_NOISE_AND_AVERAGE);

    for (int i = 0; i < 5; i++) {
        const uint16_t result = adc_filter_apply(&filter, 2048);
        TEST_ASSERT_TRUE(result <= TEST_MAX_RAW_VALUE);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testAdcFilterInit);
    RUN_TEST(testAdcFilterSetModeAndGetMode);
    RUN_TEST(testAdcFilterApply);
    RUN_TEST(testAdcFilterSetModeResetsMovingAverage);
    RUN_TEST(testAdcFilterSetModeIsIdempotent);
    RUN_TEST(testAdcFilterApplyNoise);
    RUN_TEST(testAdcFilterApplyNoiseAndAverage);
    return UNITY_END();
}
