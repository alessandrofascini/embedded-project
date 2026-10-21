/*!
 * \file adc-filter.h
 * \author Alessandro Fascini
 * \brief Types of the ADC filter: the AdcFilter instance, its modes (states), events and error codes.
 */

#ifndef RECRUITING_ADC_FILTER_H
#define RECRUITING_ADC_FILTER_H

#include <stdint.h>

#include "adc-moving-average.h"

#define ADC_FILTER_WINDOW_SIZE (150)
#define ADC_FILTER_INVALID_OPERATION (UINT16_MAX)

struct AdcFilter;

/*!
 * \brief Function applying a specific transformation chain to a raw ADC sample.
 * \param self       Owning AdcFilter instance (holds any per-channel state the transform needs).
 * \param raw_sample Raw ADC sample to transform.
 * \return Filtered sample.
 */
typedef uint16_t (*adc_filter_apply_fn)(struct AdcFilter *self, uint16_t raw_sample);

/*!
 * \brief Bitmask selecting which transformations are applied to a raw ADC sample before use.
 */
enum AdcFilterMode {
    ADC_FILTER_RAW = 0,                                                          /*!< 00 - No transformation (raw ADC value). */
    ADC_FILTER_NOISE = 1 << 0,                                                   /*!< 01 - Inject configurable random noise. */
    ADC_FILTER_MOVING_AVERAGE = 1 << 1,                                          /*!< 10 - Apply a moving-average filter. */
    ADC_FILTER_NOISE_AND_AVERAGE = ADC_FILTER_NOISE | ADC_FILTER_MOVING_AVERAGE, /*!< 11 - Both, noise injected before averaging. */
    ADC_FILTER_ERROR_MODE = -1,                                                  /*!< -1 - Error mode. Should never used */
    ADC_FILTER_UNKNOWN_MODE = -2,                                                /*!< -2 - Unknown mode. Should never used */
};

/*!
 * \brief Events driving the AdcFilter state machine (whose states are enum AdcFilterMode).
 *
 * Filters are cumulative: ADD events enable a filter on top of the active ones,
 * CLEAR is the only way to disable them.
 */
enum AdcFilterEvent {
    ADC_FILTER_EVENT_CLEAR,       /*!< Disable every filter (go to ADC_FILTER_RAW). */
    ADC_FILTER_EVENT_ADD_NOISE,   /*!< Enable noise injection on top of the active filters. */
    ADC_FILTER_EVENT_ADD_AVERAGE, /*!< Enable the moving average on top of the active filters. */
};

/*!
 * \brief Error codes for the ADC filter.
 */
enum AdcFilterError {
    ADC_FILTER_OK,                        /*!< Initialization succeeded. */
    ADC_FILTER_SELF_IS_NULL,              /*!< self is NULL. */
    ADC_FILTER_MOVING_AVERAGE_INIT_FAILED /*!< The underlying moving-average filter failed to initialize. */
};

/*!
 * \brief Per-channel ADC sample filter: composes noise injection and moving average.
 */
struct AdcFilter {
    adc_filter_apply_fn apply;                              /*!< Function pointer to the filter's apply function. */
    struct AdcMovingAverage moving_average;                 /*!< Moving average filter instance. */
    uint16_t moving_average_buffer[ADC_FILTER_WINDOW_SIZE]; /*!< Buffer for the moving average filter. */
};

#endif