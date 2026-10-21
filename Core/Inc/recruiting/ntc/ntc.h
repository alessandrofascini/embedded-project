/*!
 * \file ntc.h
 * \author Alessandro Fascini
 * \brief Types of the NTC thermistor model: the Ntc instance.
 */

#ifndef RECRUITING_NTC_H
#define RECRUITING_NTC_H

#include <stdint.h>

#define NTC_ADC_MAX_VALUE (4095U)

/*!
 * \brief Circuit parameters of an NTC thermistor, read once by ntc_init().
 */
struct NtcParams {
    float r_pullup;  /*!< Divider pull-up/series resistor, ohms. */
    float r0_ohms;   /*!< NTC nominal resistance at t0_kelvin, ohms. */
    float t0_kelvin; /*!< Nominal temperature, kelvin (usually 298.15 = 25 degC). */
    float beta;      /*!< Beta coefficient, kelvin. */
};

/*!
 * \brief Represents an NTC thermistor wired in a voltage-divider circuit.
 *
 * Only the terms of the Beta model that do not depend on the ADC reading are
 * kept, precomputed by ntc_init() so that a conversion needs no division by
 * the circuit parameters:
 *
 *     1/T = inverse_t_offset + inverse_beta * ln(NTC_ADC_MAX_VALUE / raw - 1)
 */
struct Ntc {
    float inverse_t_offset; /*!< 1/t0_kelvin + ln(r_pullup / r0_ohms) / beta, 1/kelvin. */
    float inverse_beta;     /*!< 1/beta, 1/kelvin. */
};

#endif
