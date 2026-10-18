/*!
 * \file ntc-api.c
 * \author Alessandro Fascini
 *
 * \brief Implementation of an NTC thermistor.
 */
#include "ntc-api.h"

#include <math.h>
#include <stddef.h>

#define KELVIN_TO_CELSIUS_OFFSET (273.15f)

enum NtcError ntc_init(struct Ntc *self, const float r_pullup, const float r0, const float t0_kelvin, const float beta) {
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

    /* Beta model, with R = r_pullup * ratio and ratio = NTC_ADC_MAX_VALUE / raw - 1:
     *   1/T = 1/t0 + ln(R / r0) / beta
     *       = [1/t0 + ln(r_pullup / r0) / beta] + ln(ratio) / beta
     * The bracket does not depend on the reading, so it is computed once here. */
    self->inverse_beta = 1.0f / beta;
    self->inverse_t_offset = 1.0f / t0_kelvin + logf(r_pullup / r0) * self->inverse_beta;
    return NTC_OK;
}

/*!
 * \brief Divider ratio R / r_pullup for a raw ADC reading.
 * \param raw_adc Raw ADC reading, in (0, NTC_ADC_MAX_VALUE).
 * \return NTC_ADC_MAX_VALUE / raw_adc - 1.
 */
static inline float prv_adc_to_divider_ratio(const uint16_t raw_adc) {
    // TODO: make NTC_ADC_MAX_VALUE a parameter of the NTC struct, or pass it as an argument to this function
    return NTC_ADC_MAX_VALUE / (float)raw_adc - 1.0f;
}

/*!
 * \brief Temperature for a given divider ratio, using the precomputed Beta model terms.
 * \param self  NTC instance to use.
 * \param ratio Divider ratio, from prv_adc_to_divider_ratio().
 * \return Temperature in kelvin.
 */
static inline float prv_divider_ratio_to_kelvin(const struct Ntc *self, const float ratio) {
    return 1.0f / (self->inverse_t_offset + self->inverse_beta * logf(ratio));
}

float ntc_raw_adc_to_celsius(const struct Ntc *self, const uint16_t raw_adc) {
    if (self == NULL) {
        return NAN;
    }
    if (raw_adc == 0 || raw_adc >= NTC_ADC_MAX_VALUE) {
        return INFINITY; // Open circuit
    }
    const float ratio = prv_adc_to_divider_ratio(raw_adc);
    const float kelvin = prv_divider_ratio_to_kelvin(self, ratio);
    if (isnan(kelvin)) {
        return NAN;
    }
    return kelvin - KELVIN_TO_CELSIUS_OFFSET;
}
