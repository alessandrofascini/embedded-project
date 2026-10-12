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
 * \brief Set the mode of the ADC filter.
 * \param self The AdcFilter instance to configure.
 * \param mode The mode to set.
 */
void adc_filter_set_mode(struct AdcFilter *self, const enum AdcFilterMode mode);

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
uint16_t adc_filter_apply(struct AdcFilter *self, const uint16_t raw_sample);

#endif