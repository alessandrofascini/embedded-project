#ifndef RECRUITING_ADC_MOVING_AVERAGE_H
#define RECRUITING_ADC_MOVING_AVERAGE_H

#include <stddef.h>
#include <stdint.h>

#define ADC_MOVING_AVERAGE_NO_DATA (0xFFFFU)

/*!
 * \brief Moving-average filter over a caller-owned buffer of raw ADC samples.
 */
struct AdcMovingAverage {
    uint16_t *buffer;   /*!< Caller-owned backing storage, capacity elements. */
    size_t capacity;    /*!< Number of slots in buffer (window size). */
    uint32_t sum;       /*!< Running sum of buffer's contents. */
    size_t index;       /*!< Next slot to write in buffer. */
    size_t count;       /*!< Samples written so far, saturates at capacity. */
};

#endif
