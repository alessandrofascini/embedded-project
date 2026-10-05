#ifndef RECRUITING_ADC_MOVING_AVERAGE_API_H
#define RECRUITING_ADC_MOVING_AVERAGE_API_H

#include "adc-moving-average.h"

/*!
 * \brief Result codes for adc_moving_average_init().
 */
enum AdcMovingAverageError {
    ADC_MOVING_AVERAGE_OK = 0,              /*!< Initialization succeeded. */
    ADC_MOVING_AVERAGE_SELF_IS_NULL = -1,   /*!< self is NULL. */
    ADC_MOVING_AVERAGE_BUFFER_IS_NULL = -2, /*!< self or buffer is NULL. */
    ADC_MOVING_AVERAGE_ZERO_CAPACITY = -3   /*!< capacity is 0. */
};

/*!
 * \brief Initialize an AdcMovingAverage filter instance to its empty state.
 * \param self     Instance to initialize.
 * \param buffer   Caller-owned backing storage for the ring buffer.
 * \param capacity Number of elements in buffer (window size).
 * \return ADC_MOVING_AVERAGE_OK on success, or one of the other enum AdcMovingAverageError values on failure.
 */
enum AdcMovingAverageError adc_moving_average_init(struct AdcMovingAverage *self, uint16_t *buffer, size_t capacity);

/*!
 * \brief Add a new raw ADC sample to the moving-average filter.
 * \param self   Instance previously initialized with adc_moving_average_init().
 * \param sample Raw ADC sample.
 */
void adc_moving_average_add_sample(struct AdcMovingAverage *self, const uint16_t sample);

/*!
 * \brief Get the current moving average of the raw ADC samples.
 * \param self Instance previously initialized with adc_moving_average_init().
 * \return Average of the samples currently held, or NAN if none were added yet.
 */
float adc_moving_average_get(const struct AdcMovingAverage *self);

/*!
 * \brief Get the last raw ADC sample added.
 * \param self Instance previously initialized with adc_moving_average_init().
 * \return Last raw sample added, or ADC_MOVING_AVERAGE_NO_DATA if none were added yet.
 */
uint16_t adc_moving_average_get_last_sample(const struct AdcMovingAverage *self);

#endif
