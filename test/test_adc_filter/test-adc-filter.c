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
    * the other tests here that rely on the real moving average.
    */
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testAdcFilterInit);
    RUN_TEST(testAdcFilterSetModeAndGetMode);
    RUN_TEST(testAdcFilterApply);
    return UNITY_END();
}
