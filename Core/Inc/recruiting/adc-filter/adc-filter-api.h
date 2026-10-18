/*!
 * \file adc-filter-api.h
 * \author Alessandro Fascini
 * \brief API of the ADC filter: a per-channel state machine combining noise injection and a moving average.
 */

#ifndef RECRUITING_ADC_FILTER_API_H
#define RECRUITING_ADC_FILTER_API_H

#include <stdint.h>

#include "adc-filter.h"

/*!
 * \brief Initialize the ADC filter.
 * \param self The AdcFilter instance to initialize.
 * \return     The error code of the initialization. 
 */
enum AdcFilterError adc_filter_init(struct AdcFilter *self);

/*!
 * \brief Feed an event to the filter's state machine.
 *
 * | state \ event     | CLEAR | ADD_NOISE         | ADD_AVERAGE         |
 * |-------------------|-------|-------------------|---------------------|
 * | RAW               | RAW   | NOISE             | MOVING_AVERAGE      |
 * | NOISE             | RAW   | NOISE             | NOISE_AND_AVERAGE   |
 * | MOVING_AVERAGE    | RAW   | NOISE_AND_AVERAGE | MOVING_AVERAGE      |
 * | NOISE_AND_AVERAGE | RAW   | NOISE_AND_AVERAGE | NOISE_AND_AVERAGE   |
 *
 * A corrupted instance (ADC_FILTER_UNKNOWN_MODE) only accepts CLEAR, which recovers it.
 *
 * \param self  The AdcFilter instance to update.
 * \param event The event to handle.
 */
void adc_filter_handle_event(struct AdcFilter *self, enum AdcFilterEvent event);

/*!
 * \brief Get the mode of the ADC filter.
 * \param self The AdcFilter instance to query.
 * \return     The current mode of the filter.
 */
enum AdcFilterMode adc_filter_get_mode(const struct AdcFilter *self);

/*!
 * \brief Apply the ADC filter to a raw sample.
 * \param self       The AdcFilter instance to use.
 * \param raw_sample The raw ADC sample to filter.
 * \return           The filtered ADC sample.
 */
uint16_t adc_filter_apply(struct AdcFilter *self, uint16_t raw_sample);

#endif