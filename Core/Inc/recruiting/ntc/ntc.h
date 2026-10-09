#ifndef RECRUITING_NTC_H
#define RECRUITING_NTC_H

#include <stdint.h>

#include "adc-moving-average.h"

#define NTC_AVG_WINDOW (150U)
#define NTC_ADC_MAX_VALUE (4095U)

/*!
 * \brief Represents an NTC thermistor wired in a voltage-divider circuit,
 *        with a moving-average filter over its own raw ADC samples.
 */
struct NTC {
    float r_pullup;                  /*!< Divider pull-up/series resistor, ohms. */
    float r0;                        /*!< NTC nominal resistance at t0_kelvin, ohms. */
    float t0_kelvin;                 /*!< Nominal temperature, kelvin (usually 298.15 = 25 degC). */
    float beta;                      /*!< Beta coefficient, kelvin. */
    float vcc_mv;                    /*!< Supply voltage feeding the divider, millivolts. */
    struct AdcMovingAverage filter;  /*!< Moving-average filter over this NTC's raw ADC samples. */
    uint16_t buffer[NTC_AVG_WINDOW]; /*!< Backing storage for filter's ring buffer. */
};

#endif
