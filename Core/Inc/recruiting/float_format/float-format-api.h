#ifndef RECRUITING_FLOAT_FORMAT_API_H
#define RECRUITING_FLOAT_FORMAT_API_H

#include "float-format.h"

/*!
 * \brief Result codes for float_to_string().
 */
enum FloatFormatError {
    FLOAT_FORMAT_OK = 0,                    /*!< Conversion succeeded. */
    FLOAT_FORMAT_ERR_NULL = -1,             /*!< buffer is NULL or buffer_size is 0. */
    FLOAT_FORMAT_ERR_NAN = -2,              /*!< value is NaN, cannot be formatted. */
    FLOAT_FORMAT_ERR_BUFFER_TOO_SMALL = -3, /*!< buffer_size too small to hold even "0.0". */
    FLOAT_FORMAT_ERR_OVERFLOW = -4          /*!< buffer_size too small to fit the sign, all digits, and the terminator. */
};

/*!
 * \brief Convert a float into a fixed one-decimal-digit string (e.g. "12.3", "-4.0").
 * \param value       The float value to convert. May be negative.
 * \param buffer      Destination buffer to write the result into.
 * \param buffer_size Size of buffer, in bytes.
 * \return FLOAT_FORMAT_OK on success, or one of the other enum FloatFormatError values on failure.
 */
enum FloatFormatError float_to_string(float value, char *buffer, uint16_t buffer_size);

#endif