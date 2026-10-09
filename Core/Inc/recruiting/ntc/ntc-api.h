#ifndef RECRUITING_NTC_API_H
#define RECRUITING_NTC_API_H

#include "ntc.h"

/*!
 * \brief Result codes for ntc_init().
 */
enum NtcError {
    NTC_OK = 0,                             /*!< Initialization succeeded. */
    NTC_SELF_IS_NULL = -1,                  /*!< self is NULL. */
    NTC_INVALID_R_PULLUP = -2,              /*!< r_pullup is <= 0. */
    NTC_INVALID_R0 = -3,                    /*!< r0 is <= 0. */
    NTC_INVALID_T0 = -4,                    /*!< t0_kelvin is <= 0. */
    NTC_INVALID_BETA = -5,                  /*!< beta is <= 0. */
    NTC_INVALID_VCC = -6,                   /*!< vcc_mv is <= 0. */
    NTC_ADC_MOVING_AVERAGE_INIT_FAILED = -7 /*!< adc_moving_average_init() failed. */
};

/*!
 * \brief Initialize an NTC instance to its empty state with the given circuit parameters.
 * \param self      Instance to initialize.
 * \param r_pullup  Divider pull-up/series resistor, ohms.
 * \param r0        NTC nominal resistance at t0_kelvin, ohms.
 * \param t0_kelvin Nominal temperature, kelvin (usually 298.15 = 25 degC).
 * \param beta      Beta coefficient, kelvin.
 * \param vcc_mv    Supply voltage feeding the divider, millivolts.
 * \return NTC_OK on success, or one of the other enum NtcError values on failure.
 */
enum NtcError ntc_init(struct NTC *self, const float r_pullup, const float r0, const float t0_kelvin, const float beta, const float vcc_mv);

/*!
 * \brief Add a new raw ADC sample to the NTC's moving-average filter.
 * \param self    Instance previously initialized with ntc_init().
 * \param raw_adc Raw ADC value.
 */
void ntc_add_sample(struct NTC *self, const uint16_t raw_adc);

/*!
 * \brief Convert a raw ADC value into the NTC's resistance, given its divider circuit.
 * \param self    Instance previously initialized with ntc_init().
 * \param raw_adc Raw ADC value.
 * \return NTC resistance in ohms.
 */
float ntc_raw_adc_to_resistance(const struct NTC *self, const uint16_t raw_adc);

/*!
 * \brief Convert an NTC resistance into a temperature, using the Beta equation.
 * \param self       Instance previously initialized with ntc_init().
 * \param resistance NTC resistance in ohms.
 * \return Temperature in kelvin.
 */
float ntc_resistance_to_kelvin(const struct NTC *self, const float resistance);

/*!
 * \brief Convert an NTC resistance into a temperature, using the Beta equation.
 * \param self       Instance previously initialized with ntc_init().
 * \param resistance NTC resistance in ohms.
 * \return Temperature in degrees Celsius. Equivalent to ntc_resistance_to_kelvin() minus 273.15.
 */
float ntc_resistance_to_celsius(const struct NTC *self, const float resistance);

/*!
 * \brief Get the current moving average of the raw ADC samples.
 * \param self Instance previously initialized with ntc_init().
 * \return Average of the raw ADC samples currently held, or NAN if none were added yet.
 */
float ntc_get_sample_average(const struct NTC *self);

/*!
 * \brief Get the last raw ADC sample added.
 * \param self Instance previously initialized with ntc_init().
 * \return Last raw ADC sample added, or ADC_MOVING_AVERAGE_NO_DATA if none were added yet.
 */
uint16_t ntc_get_last_sample(const struct NTC *self);

/*!
 * \brief Get the current moving average of the raw ADC samples, converted to kelvin.
 * \param self Instance previously initialized with ntc_init().
 * \return Moving average temperature in kelvin, or NAN if no sample was added yet.
 */
float ntc_get_kelvin(const struct NTC *self);

/*!
 * \brief Get the current moving average of the raw ADC samples, converted to degrees Celsius.
 * \param self Instance previously initialized with ntc_init().
 * \return Moving average temperature in degrees Celsius, or NAN if no sample was added yet.
 */
float ntc_get_celsius(const struct NTC *self);

/*!
 * \brief Get the last raw ADC value added, converted to kelvin.
 * \param self Instance previously initialized with ntc_init().
 * \return Last temperature reading in kelvin, or NAN if no sample was added yet.
 */
float ntc_get_last_kelvin(const struct NTC *self);

/*!
 * \brief Get the last raw ADC value added, converted to degrees Celsius.
 * \param self Instance previously initialized with ntc_init().
 * \return Last temperature reading in degrees Celsius, or NAN if no sample was added yet.
 */
float ntc_get_last_celsius(const struct NTC *self);

#endif
