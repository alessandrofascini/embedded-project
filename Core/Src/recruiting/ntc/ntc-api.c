/*!
 * \file ntc-api.c
 * \author Alessandro Fascini
 *
 * \brief Implementation of an NTC thermistor.
 */
#include "ntc-api.h"

#include <math.h>

#define KELVIN_TO_CELSIUS_OFFSET (273.15f)

enum NtcError ntc_init(struct NTC *self, const float r_pullup, const float r0, const float t0_kelvin, const float beta) {
    // TODO: too many parameters - consider refactoring
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

    self->r_pullup = r_pullup;
    self->r0 = r0;
    self->t0_kelvin = t0_kelvin;
    self->beta = beta;
    return NTC_OK;
}

static float prv_adc_to_resistance(const struct NTC *self, const uint16_t raw_adc) {
    // TODO: make NTC_ADC_MAX_VALUE a parameter of the NTC struct, or pass it as an argument to this function
    return self->r_pullup * (NTC_ADC_MAX_VALUE / (float)raw_adc - 1.0f);
}

static float prv_raw_adc_to_kelvin(const struct NTC *self, const float resistance) {
    // TODO: after initialization, precompute 1/t0_kelvin and 1/beta to avoid repeated divisions
    const float inverse_t0 = 1.0f / self->t0_kelvin;
    const float inverse_beta = 1.0f / self->beta;
    const float ln_r_ratio = logf(resistance / self->r0);
    return 1.0f / (inverse_t0 + inverse_beta * ln_r_ratio);
}

float ntc_raw_adc_to_celsius(const struct NTC *self, const uint16_t raw_adc) {
    if (self == NULL) {
        return NAN;
    }
    if (raw_adc == 0 || raw_adc >= NTC_ADC_MAX_VALUE) {
        return INFINITY; // Open circuit
    }
    const float resistance = prv_adc_to_resistance(self, raw_adc);
    const float kelvin = prv_raw_adc_to_kelvin(self, resistance);
    if (isnan(kelvin)) {
        return NAN;
    }
    return kelvin - KELVIN_TO_CELSIUS_OFFSET;
}
