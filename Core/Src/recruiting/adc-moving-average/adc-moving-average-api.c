/*!
 * \file adc-moving-average-api.c
 * \author Alessandro Fascini
 *
 * \brief Implementation of a generic moving-average filter over raw ADC samples.
 */
#include "adc-moving-average-api.h"

#include <math.h>

enum AdcMovingAverageError adc_moving_average_init(struct AdcMovingAverage *self, uint16_t *buffer, size_t capacity) {
    if (self == NULL) {
        return ADC_MOVING_AVERAGE_SELF_IS_NULL;
    }
    if (buffer == NULL) {
        return ADC_MOVING_AVERAGE_BUFFER_IS_NULL;
    }
    if (capacity == 0) {
        return ADC_MOVING_AVERAGE_ZERO_CAPACITY;
    }
    self->buffer = buffer;
    self->capacity = capacity;
    self->sum = 0;
    self->index = 0;
    self->count = 0;
    return ADC_MOVING_AVERAGE_OK;
}

void adc_moving_average_add_sample(struct AdcMovingAverage *self, const uint16_t sample) {
    // TODO: implement
}

float adc_moving_average_get(const struct AdcMovingAverage *self) {
    // TODO: implement
    return NAN;
}

uint16_t adc_moving_average_get_last_sample(const struct AdcMovingAverage *self) {
    // TODO: implement
    return ADC_MOVING_AVERAGE_NO_DATA;
}
