/*!
 * \file adc-filter-api.c
 * \author Alessandro Fascini
 * \brief Implementation of the ADC filter API.
 *
 * At the beginning, the idea was to use the Decorator pattern with pointers
 * to functions. However, this pattern is not very suitable for C, and I
 * also needed to handle more complex inner states, like noise -> average.
 * So I decided to map the states into a FSM and put inside the AdcFilter
 * struct a pointer to the function that implements the current state.
 * For now, I think it is the best solution, quite simple and easy to understand.
 */

#include "adc-filter-api.h"

#include "adc-moving-average-api.h"

#define ADC_FILTER_LFSR_SEED (0xACE1)
#define ADC_FILTER_NOISE_AMPLITUDE (20)
#define ADC_FILTER_MAX_RAW_VALUE (4095)
#define ADC_FILTER_NOISE_RANGE_SIZE (2 * ADC_FILTER_NOISE_AMPLITUDE + 1)

static uint16_t prv_lfsr_state = ADC_FILTER_LFSR_SEED;
static uint16_t prv_lfsr_next(void);

static uint16_t prv_mode_raw(struct AdcFilter *self, uint16_t raw_sample);
static uint16_t prv_mode_average(struct AdcFilter *self, uint16_t raw_sample);
static uint16_t prv_mode_noise(struct AdcFilter *self, uint16_t raw_sample);
static uint16_t prv_mode_noise_and_average(struct AdcFilter *self, uint16_t raw_sample);

enum AdcFilterError adc_filter_init(struct AdcFilter *self) {
    if (self == NULL) {
        return ADC_FILTER_SELF_IS_NULL;
    }
    self->apply = prv_mode_raw;
    const uint16_t buffer_size = sizeof(self->moving_average_buffer) / sizeof(self->moving_average_buffer[0]);
    const enum AdcMovingAverageError error = adc_moving_average_init(&self->moving_average, self->moving_average_buffer, buffer_size);
    if (error != ADC_MOVING_AVERAGE_OK) {
        return ADC_FILTER_MOVING_AVERAGE_INIT_FAILED;
    }
    return ADC_FILTER_OK;
}

void adc_filter_set_mode(struct AdcFilter *self, const enum AdcFilterMode mode) {
    if (self == NULL) {
        return;
    }
    switch (mode) {
        case ADC_FILTER_MOVING_AVERAGE:
            self->apply = prv_mode_average;
            break;
        case ADC_FILTER_NOISE:
            self->apply = prv_mode_noise;
            break;
        case ADC_FILTER_NOISE_AND_AVERAGE:
            self->apply = prv_mode_noise_and_average;
            break;
        case ADC_FILTER_RAW:
        default:
            self->apply = prv_mode_raw; /* Default to raw mode */
            break;
    }
}

enum AdcFilterMode adc_filter_get_mode(const struct AdcFilter *self) {
    if (self == NULL) {
        return ADC_FILTER_ERROR_MODE;
    }
    if (self->apply == prv_mode_raw) {
        return ADC_FILTER_RAW;
    }
    if (self->apply == prv_mode_average) {
        return ADC_FILTER_MOVING_AVERAGE;
    }
    if (self->apply == prv_mode_noise) {
        return ADC_FILTER_NOISE;
    }
    if (self->apply == prv_mode_noise_and_average) {
        return ADC_FILTER_NOISE_AND_AVERAGE;
    }
    return ADC_FILTER_ERROR_MODE;
}

uint16_t adc_filter_apply(struct AdcFilter *self, const uint16_t raw_sample) {
    if (self == NULL || self->apply == NULL) {
        return ADC_FILTER_INVALID_OPERATION;
    }
    return self->apply(self, raw_sample);
}

/*!
 * \brief Apply a raw ADC sample.
 * \param self The ADC filter instance.
 * \param raw_sample The raw ADC sample.
 * \return The raw sample.
 */
static uint16_t prv_mode_raw(struct AdcFilter *self, uint16_t raw_sample) {
    /* self is unused here, so it is not validated — nothing in this
     * function can be affected by it being NULL. */
    return raw_sample;
}

/*!
 * \brief Apply a moving average filter to a raw ADC sample.
 * \param self The ADC filter instance.
 * \param raw_sample The raw ADC sample.
 * \return The averaged sample.
 */
static uint16_t prv_mode_average(struct AdcFilter *self, uint16_t raw_sample) {
    if (self == NULL) {
        return ADC_FILTER_INVALID_OPERATION;
    }
    adc_moving_average_add_sample(&self->moving_average, raw_sample);
    return (uint16_t)adc_moving_average_get(&self->moving_average);
}

/*!
 * \brief Advance the shared 16-bit Fibonacci LFSR by one step.
 *       Uses taps at bits 0, 2, 3, 5 (polynomial x^16+x^14+x^13+x^11+1),
 *       a maximal-period polynomial for a 16-bit LFSR (period 65535).
 * \return The LFSR's new state, usable as a 16-bit pseudo-random value.
 */
static uint16_t prv_lfsr_next(void) {
    uint16_t bit = prv_lfsr_state;
    bit ^= prv_lfsr_state >> 2;
    bit ^= prv_lfsr_state >> 3;
    bit ^= prv_lfsr_state >> 5;
    bit &= 1;
    prv_lfsr_state = (prv_lfsr_state >> 1) | (bit << 15);
    return prv_lfsr_state;
}

/*!
 * \brief Apply noise to a raw ADC sample.
 * \param self The ADC filter instance.
 * \param raw_sample The raw ADC sample.
 * \return The noisy sample.
 */
static uint16_t prv_mode_noise(struct AdcFilter *self, uint16_t raw_sample) {
    /* self is unused here, so it is not validated — nothing in this
    * function can be affected by it being NULL. */
    /* The modulo introduces a tiny bias (65535 % ADC_FILTER_NOISE_RANGE_SIZE != 0),
    * so a few offsets are very slightly more likely than others. Negligible for
    * demonstrative noise on a 12-bit ADC value; not worth the complexity of
    * rejection sampling here. */
    const int32_t offset = (int32_t)(prv_lfsr_next() % ADC_FILTER_NOISE_RANGE_SIZE) - ADC_FILTER_NOISE_AMPLITUDE;
    int32_t noisy_sample = (int32_t)raw_sample + offset;
    if (noisy_sample < 0) {
        noisy_sample = 0;
    } else if (noisy_sample > ADC_FILTER_MAX_RAW_VALUE) {
        noisy_sample = ADC_FILTER_MAX_RAW_VALUE;
    }
    return (uint16_t)noisy_sample;
}

/*!
 * \brief Apply noise and averaging filters to a raw ADC sample.
 * \param self The ADC filter instance.
 * \param raw_sample The raw ADC sample.
 * \return The filtered sample.
 */
static uint16_t prv_mode_noise_and_average(struct AdcFilter *self, uint16_t raw_sample) {
    /* self may be NULL here without issue: prv_mode_noise() ignores it,
    * and prv_mode_average() validates it before dereferencing it. */
    const uint16_t noisy_sample = prv_mode_noise(self, raw_sample);
    return prv_mode_average(self, noisy_sample);
}