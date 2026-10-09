/*!
 * \file ntc-api.c
 * \author Alessandro Fascini
 *
 * \brief Implementation of an NTC thermistor with a moving-average filter over its raw ADC samples.
 */
#include "ntc-api.h"

#include <math.h>

#include "adc-moving-average-api.h"

// TODO: too many parameters - consider refactoring
enum NtcError ntc_init(struct NTC *self, const float r_pullup, const float r0, const float t0_kelvin, const float beta, const float vcc_mv) {
    if (self == NULL) {
        return NTC_SELF_IS_NULL;
    }

    if (r_pullup <= 0.0f) {
        return NTC_INVALID_R_PULLUP;
    }
    if (r0 <= 0.0f) {
        return NTC_INVALID_R0;
    }
    if (t0_kelvin <= 0.0f) {
        return NTC_INVALID_T0;
    }
    if (beta <= 0.0f) {
        return NTC_INVALID_BETA;
    }
    if (vcc_mv <= 0.0f) {
        return NTC_INVALID_VCC;
    }
    const enum AdcMovingAverageError result = adc_moving_average_init(&self->filter, self->buffer, NTC_AVG_WINDOW);
    if (result != ADC_MOVING_AVERAGE_OK) {
        // TODO: consider hanling the errors better
        return NTC_ADC_MOVING_AVERAGE_INIT_FAILED;
    }

    self->r_pullup = r_pullup;
    self->r0 = r0;
    self->t0_kelvin = t0_kelvin;
    self->beta = beta;
    self->vcc_mv = vcc_mv;
    return NTC_OK;
}

void ntc_add_sample(struct NTC *self, const uint16_t raw_adc) {
    if (self == NULL) {
        return;
    }
    adc_moving_average_add_sample(&self->filter, raw_adc);
}

float ntc_raw_adc_to_resistance(const struct NTC *self, const uint16_t raw_adc) {
    // TODO: implement
    return NAN;
}

float ntc_resistance_to_kelvin(const struct NTC *self, const float resistance) {
    // TODO: implement
    return NAN;
}

float ntc_resistance_to_celsius(const struct NTC *self, const float resistance) {
    // TODO: implement - wrap ntc_resistance_to_kelvin(), subtracting 273.15
    return NAN;
}

float ntc_get_sample_average(const struct NTC *self) {
    // TODO: implement - passthrough to adc_moving_average_get(&self->filter)
    return NAN;
}

uint16_t ntc_get_last_sample(const struct NTC *self) {
    // TODO: implement - passthrough to adc_moving_average_get_last_sample(&self->filter)
    return ADC_MOVING_AVERAGE_NO_DATA;
}

float ntc_get_kelvin(const struct NTC *self) {
    // TODO: implement - ntc_get_sample_average() -> ntc_raw_adc_to_resistance() -> ntc_resistance_to_kelvin()
    return NAN;
}

float ntc_get_celsius(const struct NTC *self) {
    // TODO: implement - ntc_get_sample_average() -> ntc_raw_adc_to_resistance() -> ntc_resistance_to_celsius()
    return NAN;
}

float ntc_get_last_kelvin(const struct NTC *self) {
    // TODO: implement - ntc_get_last_sample() -> ntc_raw_adc_to_resistance() -> ntc_resistance_to_kelvin()
    return NAN;
}

float ntc_get_last_celsius(const struct NTC *self) {
    // TODO: implement - ntc_get_last_sample() -> ntc_raw_adc_to_resistance() -> ntc_resistance_to_celsius()
    return NAN;
}
