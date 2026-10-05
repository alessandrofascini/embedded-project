/*!
 * \file test-adc-moving-average.c
 * \author Alessandro Fascini
 *
 * \brief AdcMovingAverage test file.
 */

#include "unity.h"
#include "adc-moving-average-api.h"
// Include the FFF library for function mocking (must be pasted into test/include/fff.h)
#include "fff.h"

void testAdcMovingAverageInit(void) {
    struct AdcMovingAverageInitCase {
        struct AdcMovingAverage *self;
        uint16_t *buffer;
        size_t capacity;
        enum AdcMovingAverageError expected_result;
    };

    struct AdcMovingAverage SUT;

    uint16_t buffer[4];
    const size_t buffer_size = sizeof(buffer) / sizeof(buffer[0]);

    const struct AdcMovingAverageInitCase test_cases[] = {
        { NULL, NULL, 0, ADC_MOVING_AVERAGE_SELF_IS_NULL },
        { &SUT, NULL, 0, ADC_MOVING_AVERAGE_BUFFER_IS_NULL },
        { &SUT, buffer, 0, ADC_MOVING_AVERAGE_ZERO_CAPACITY },
        { &SUT, buffer, buffer_size, ADC_MOVING_AVERAGE_OK },
    };

    const size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    for (size_t i = 0; i < num_cases; i++) {
        const struct AdcMovingAverageInitCase *c = &test_cases[i];
        enum AdcMovingAverageError result = adc_moving_average_init(c->self, c->buffer, c->capacity);
        TEST_ASSERT_EQUAL_INT(c->expected_result, result);
    }

    enum AdcMovingAverageError result = adc_moving_average_init(&SUT, buffer, buffer_size);
    TEST_ASSERT_EQUAL_INT(ADC_MOVING_AVERAGE_OK, result);
    TEST_ASSERT_EQUAL_PTR(buffer, SUT.buffer);
    TEST_ASSERT_EQUAL_INT(buffer_size, SUT.capacity);
    TEST_ASSERT_EQUAL_UINT32(0, SUT.sum);
    TEST_ASSERT_EQUAL_INT(0, SUT.index);
    TEST_ASSERT_EQUAL_INT(0, SUT.count);
    TEST_ASSERT_EQUAL_UINT16(0, SUT.buffer[0]); // Check that the first element is initialized to 0
}

void testAdcMovingAverageAddSample(void) {
    struct AdcMovingAverageAddSampleCase {
        uint16_t samples[8];         // samples to add, in order
        size_t num_samples;          // how many of the above to actually add
        size_t buffer_size;          // capacity of the buffer to init the AdcMovingAverage with
        uint32_t expected_sum;       // self->sum expected after adding them
        size_t expected_index;       // self->index expected after adding them
        size_t expected_count;       // self->count expected after adding them
        uint16_t expected_buffer[8]; // self->buffer contents expected after adding the samples
    };

    const struct AdcMovingAverageAddSampleCase test_cases[] = {
        { { 1, 2, 3, 4 }, 4, 4, 10, 0, 4, { 1, 2, 3, 4 } },
        { { 5, 6, 7, 8 }, 4, 4, 26, 0, 4, { 5, 6, 7, 8 } },
        { { 9 }, 1, 1, 9, 0, 1, { 9 } },
        { { 1, 2, 3, 4, 5 }, 5, 4, 14, 1, 4, { 5, 2, 3, 4 } },
        { { 1, 2, 3, 4, 5, 6, 7 }, 7, 4, 22, 3, 4, { 5, 6, 7, 4 } }
    };

    const size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (size_t i = 0; i < num_cases; i++) {
        const struct AdcMovingAverageAddSampleCase *c = &test_cases[i];

        const size_t buffer_size = c->buffer_size;
        struct AdcMovingAverage SUT;
        uint16_t buffer[buffer_size];

        enum AdcMovingAverageError result = adc_moving_average_init(&SUT, buffer, buffer_size);
        TEST_ASSERT_EQUAL_INT(ADC_MOVING_AVERAGE_OK, result);

        for (size_t j = 0; j < c->num_samples; j++) {
            adc_moving_average_add_sample(&SUT, c->samples[j]);
        }

        TEST_ASSERT_EQUAL_UINT32(c->expected_sum, SUT.sum);
        TEST_ASSERT_EQUAL_INT(c->expected_index, SUT.index);
        TEST_ASSERT_EQUAL_INT(c->expected_count, SUT.count);

        for (size_t k = 0; k < c->buffer_size; k++) {
            TEST_ASSERT_EQUAL_UINT16(c->expected_buffer[k], buffer[k]);
        }
    }

    // Test adding a sample to a NULL AdcMovingAverage instance

    struct AdcMovingAverage SUT;
    uint16_t buffer[4];
    adc_moving_average_init(&SUT, buffer, 4);
    adc_moving_average_add_sample(NULL, 1);
    TEST_ASSERT_EQUAL_UINT32(0, SUT.sum);
    TEST_ASSERT_EQUAL_INT(0, SUT.index);
    TEST_ASSERT_EQUAL_INT(0, SUT.count);
}

void testAdcMovingAverageGet(void) {
    struct AdcMovingAverage SUT;

    uint16_t buffer_a[4];
    adc_moving_average_init(&SUT, buffer_a, 4);
    TEST_ASSERT_FLOAT_IS_NAN(adc_moving_average_get(&SUT));

    uint16_t buffer_b[4];
    adc_moving_average_init(&SUT, buffer_b, 4);
    adc_moving_average_add_sample(&SUT, 1);
    adc_moving_average_add_sample(&SUT, 2);
    adc_moving_average_add_sample(&SUT, 3);
    TEST_ASSERT_EQUAL_FLOAT(2.0, adc_moving_average_get(&SUT));

    adc_moving_average_add_sample(&SUT, 4);
    TEST_ASSERT_EQUAL_FLOAT(2.5, adc_moving_average_get(&SUT));

    adc_moving_average_add_sample(&SUT, 5);
    TEST_ASSERT_EQUAL_FLOAT(3.5, adc_moving_average_get(&SUT));

    TEST_ASSERT_FLOAT_IS_NAN(adc_moving_average_get(NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testAdcMovingAverageInit);
    RUN_TEST(testAdcMovingAverageAddSample);
    RUN_TEST(testAdcMovingAverageGet);
    return UNITY_END();
}
