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

/*!
 * \brief Drive a freshly initialized filter into the given state through events only.
 * \param filter The filter to initialize and drive.
 * \param mode   The state to reach.
 */
static void prv_reach_mode(struct AdcFilter *filter, const enum AdcFilterMode mode) {
    adc_filter_init(filter);
    if ((mode & ADC_FILTER_NOISE) != 0) {
        adc_filter_handle_event(filter, ADC_FILTER_EVENT_ADD_NOISE);
    }
    if ((mode & ADC_FILTER_MOVING_AVERAGE) != 0) {
        adc_filter_handle_event(filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    }
    TEST_ASSERT_EQUAL(mode, adc_filter_get_mode(filter));
}

void testAdcFilterHandleEventTransitions(void) {
    /* Exhaustive state x event table; must match the one documented in adc-filter-api.h */
    const struct {
        enum AdcFilterMode from;
        enum AdcFilterEvent event;
        enum AdcFilterMode to;
    } transitions[] = {
        { ADC_FILTER_RAW, ADC_FILTER_EVENT_CLEAR, ADC_FILTER_RAW },
        { ADC_FILTER_RAW, ADC_FILTER_EVENT_ADD_NOISE, ADC_FILTER_NOISE },
        { ADC_FILTER_RAW, ADC_FILTER_EVENT_ADD_AVERAGE, ADC_FILTER_MOVING_AVERAGE },
        { ADC_FILTER_NOISE, ADC_FILTER_EVENT_CLEAR, ADC_FILTER_RAW },
        { ADC_FILTER_NOISE, ADC_FILTER_EVENT_ADD_NOISE, ADC_FILTER_NOISE },
        { ADC_FILTER_NOISE, ADC_FILTER_EVENT_ADD_AVERAGE, ADC_FILTER_NOISE_AND_AVERAGE },
        { ADC_FILTER_MOVING_AVERAGE, ADC_FILTER_EVENT_CLEAR, ADC_FILTER_RAW },
        { ADC_FILTER_MOVING_AVERAGE, ADC_FILTER_EVENT_ADD_NOISE, ADC_FILTER_NOISE_AND_AVERAGE },
        { ADC_FILTER_MOVING_AVERAGE, ADC_FILTER_EVENT_ADD_AVERAGE, ADC_FILTER_MOVING_AVERAGE },
        { ADC_FILTER_NOISE_AND_AVERAGE, ADC_FILTER_EVENT_CLEAR, ADC_FILTER_RAW },
        { ADC_FILTER_NOISE_AND_AVERAGE, ADC_FILTER_EVENT_ADD_NOISE, ADC_FILTER_NOISE_AND_AVERAGE },
        { ADC_FILTER_NOISE_AND_AVERAGE, ADC_FILTER_EVENT_ADD_AVERAGE, ADC_FILTER_NOISE_AND_AVERAGE },
    };
    const size_t n = sizeof(transitions) / sizeof(transitions[0]);

    for (size_t i = 0; i < n; i++) {
        struct AdcFilter filter;
        prv_reach_mode(&filter, transitions[i].from);
        adc_filter_handle_event(&filter, transitions[i].event);
        TEST_ASSERT_EQUAL_MESSAGE(transitions[i].to, adc_filter_get_mode(&filter), "unexpected transition");
    }
}

void testAdcFilterInvalidInstances(void) {
    adc_filter_handle_event(NULL, ADC_FILTER_EVENT_ADD_NOISE); /* must not crash */
    TEST_ASSERT_EQUAL(ADC_FILTER_ERROR_MODE, adc_filter_get_mode(NULL));

    struct AdcFilter filter;
    adc_filter_init(&filter);
    filter.apply = NULL; /* simulate a corrupted instance */
    TEST_ASSERT_EQUAL(ADC_FILTER_UNKNOWN_MODE, adc_filter_get_mode(&filter));

    /* ADD events cannot fix a corrupted instance... */
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    TEST_ASSERT_EQUAL(ADC_FILTER_UNKNOWN_MODE, adc_filter_get_mode(&filter));

    /* ...but CLEAR recovers it */
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_CLEAR);
    TEST_ASSERT_EQUAL(ADC_FILTER_RAW, adc_filter_get_mode(&filter));
}

void testAdcFilterApply(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);

    TEST_ASSERT_EQUAL_UINT16(2048, adc_filter_apply(&filter, 2048));

    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    adc_filter_apply(&filter, 1);
    adc_filter_apply(&filter, 2);
    const uint16_t result = adc_filter_apply(&filter, 3);
    TEST_ASSERT_EQUAL_UINT16(2, result); /* (1+2+3)/3 = 2.0 */

    TEST_ASSERT_EQUAL_UINT16(ADC_FILTER_INVALID_OPERATION, adc_filter_apply(NULL, 100));
}

void testAdcFilterAddAverageResetsMovingAverage(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);

    adc_filter_apply(&filter, 100);
    adc_filter_apply(&filter, 200);
    const uint16_t before_switch = adc_filter_apply(&filter, 300);
    TEST_ASSERT_EQUAL_UINT16(200, before_switch); /* (100+200+300)/3 */

    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_CLEAR);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);

    /* window should be empty again, so the average of a single sample
     * equals that sample, not an average carried over from before */
    const uint16_t after_switch = adc_filter_apply(&filter, 300);
    TEST_ASSERT_EQUAL_UINT16(300, after_switch);
}

void testAdcFilterAddAverageOnNoiseResetsMovingAverage(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    adc_filter_apply(&filter, 100);
    adc_filter_apply(&filter, 200);

    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_CLEAR);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_NOISE);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);

    /* With a fresh window the result is the single noisy sample (300 +/- noise);
     * a stale window would give about (100+200+300)/3 = 200. */
    const uint16_t result = adc_filter_apply(&filter, 300);
    TEST_ASSERT_UINT16_WITHIN(TEST_NOISE_AMPLITUDE, 300, result);
}

void testAdcFilterAddAverageIsIdempotent(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);

    adc_filter_apply(&filter, 100);
    adc_filter_apply(&filter, 200);

    /* re-requesting the average must not reset the window */
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);

    const uint16_t result = adc_filter_apply(&filter, 300);
    TEST_ASSERT_EQUAL_UINT16(200, result); /* (100+200+300)/3, not reset */
}

void testAdcFilterAddNoiseKeepsMovingAverage(void) {
    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);

    adc_filter_apply(&filter, 100);
    adc_filter_apply(&filter, 200);

    /* the average was already running: adding noise must not reset its window */
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_NOISE);

    /* (100+200+(300 +/- noise))/3 = 200 +/- noise/3 */
    const uint16_t result = adc_filter_apply(&filter, 300);
    TEST_ASSERT_UINT16_WITHIN(TEST_NOISE_AMPLITUDE / 3 + 1, 200, result);
}

void testAdcFilterApplyNoise(void) {
    const uint16_t raw_value = 2048;

    struct AdcFilter filter;
    adc_filter_init(&filter);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_NOISE);

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
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_NOISE);
    adc_filter_handle_event(&filter, ADC_FILTER_EVENT_ADD_AVERAGE);

    for (int i = 0; i < 5; i++) {
        const uint16_t result = adc_filter_apply(&filter, 2048);
        TEST_ASSERT_TRUE(result <= TEST_MAX_RAW_VALUE);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testAdcFilterInit);
    RUN_TEST(testAdcFilterHandleEventTransitions);
    RUN_TEST(testAdcFilterInvalidInstances);
    RUN_TEST(testAdcFilterApply);
    RUN_TEST(testAdcFilterAddAverageResetsMovingAverage);
    RUN_TEST(testAdcFilterAddAverageOnNoiseResetsMovingAverage);
    RUN_TEST(testAdcFilterAddAverageIsIdempotent);
    RUN_TEST(testAdcFilterAddNoiseKeepsMovingAverage);
    RUN_TEST(testAdcFilterApplyNoise);
    RUN_TEST(testAdcFilterApplyNoiseAndAverage);
    return UNITY_END();
}
