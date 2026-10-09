#ifndef RECRUITING_NTC_H
#define RECRUITING_NTC_H

#include <stdint.h>

#define NTC_ADC_MAX_VALUE (4095U)

/*!
 * \brief Represents an NTC thermistor wired in a voltage-divider circuit.
 */
struct NTC {
    float r_pullup;  /*!< Divider pull-up/series resistor, ohms. */
    float r0;        /*!< NTC nominal resistance at t0_kelvin, ohms. */
    float t0_kelvin; /*!< Nominal temperature, kelvin (usually 298.15 = 25 degC). */
    float beta;      /*!< Beta coefficient, kelvin. */
};

#endif
