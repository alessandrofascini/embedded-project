/*!
 * \file ntc-api.h
 * \author Alessandro Fascini
 * \brief API of an NTC thermistor: conversion of raw ADC readings to degrees Celsius with the Beta model.
 */

#ifndef RECRUITING_NTC_API_H
#define RECRUITING_NTC_API_H

#include "ntc.h"

/*!
 * \brief Result codes for ntc_init().
 */
enum NtcError {
    NTC_OK = 0,                /*!< Initialization succeeded. */
    NTC_SELF_IS_NULL = -1,     /*!< self is NULL. */
    NTC_INVALID_R_PULLUP = -2, /*!< r_pullup is <= 0. */
    NTC_INVALID_R0 = -3,       /*!< r0_ohms is <= 0. */
    NTC_INVALID_T0 = -4,       /*!< t0_kelvin is <= 0. */
    NTC_INVALID_BETA = -5,     /*!< beta is <= 0. */
    NTC_PARAMS_IS_NULL = -6    /*!< params is NULL. */
};

/*!
 * \brief Initialize an Ntc instance to its empty state with the given circuit parameters.
 * \param self   Instance to initialize.
 * \param params Circuit parameters; only read here, so it may be a temporary.
 * \return NTC_OK on success, or one of the other enum NtcError values on failure.
 */
enum NtcError ntc_init(struct Ntc *self, const struct NtcParams *params);

/*!
 * \brief Convert a raw ADC value to a temperature in degrees Celsius.
 * \param self    NTC instance to use.
 * \param raw_adc Raw ADC value.
 * \return Temperature in degrees Celsius, or NAN if the instance is NULL, or INFINITY if the raw ADC value is 0 or NTC_ADC_MAX_VALUE.
 */
float ntc_raw_adc_to_celsius(const struct Ntc *self, uint16_t raw_adc);

#endif