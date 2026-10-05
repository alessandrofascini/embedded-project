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

    struct AdcMovingAverage self;
    uint16_t buffer[4];
    const size_t buffer_size = sizeof(buffer) / sizeof(buffer[0]);

    const struct AdcMovingAverageInitCase test_cases[] = {
        { NULL, NULL, 0, ADC_MOVING_AVERAGE_SELF_IS_NULL },
        { &self, NULL, 0, ADC_MOVING_AVERAGE_BUFFER_IS_NULL },
        { &self, buffer, 0, ADC_MOVING_AVERAGE_ZERO_CAPACITY },
        { &self, buffer, buffer_size, ADC_MOVING_AVERAGE_OK },
    };

    const size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    for (size_t i = 0; i < num_cases; i++) {
        const struct AdcMovingAverageInitCase *c = &test_cases[i];
        enum AdcMovingAverageError result = adc_moving_average_init(c->self, c->buffer, c->capacity);
        TEST_ASSERT_EQUAL_INT(c->expected_result, result);
    }

    enum AdcMovingAverageError result = adc_moving_average_init(&self, buffer, buffer_size);
    TEST_ASSERT_EQUAL_INT(ADC_MOVING_AVERAGE_OK, result);
    TEST_ASSERT_EQUAL_PTR(buffer, self.buffer);
    TEST_ASSERT_EQUAL_INT(buffer_size, self.capacity);
    TEST_ASSERT_EQUAL_UINT32(0, self.sum);
    TEST_ASSERT_EQUAL_INT(0, self.index);
    TEST_ASSERT_EQUAL_INT(0, self.count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testAdcMovingAverageInit);
    return UNITY_END();
}
