/*!
 * \file float-format-api.c
 * \author Alessandro Fascini
 *
 * \brief Implementation of a simple float formatter.
 */
#include "float-format-api.h"
#include <math.h>

#define FLOAT_FORMAT_BASE (10)

enum FloatFormatError float_to_string(float value, char *buffer, uint16_t buffer_size) {
    if (isnan(value)) {
        return FLOAT_FORMAT_ERR_NAN;
    }
    if (buffer == NULL || buffer_size == 0) {
        return FLOAT_FORMAT_ERR_NULL;
    }
    if (buffer_size < 3) {
        return FLOAT_FORMAT_ERR_BUFFER_TOO_SMALL;
    }
    int sign = 1;
    if (value < 0) {
        sign = -1;
        value = -value;
    }
    int integer_part = (int)(value * FLOAT_FORMAT_BASE);
    buffer[0] = '\0';
    buffer[1] = (char)('0' + (integer_part % FLOAT_FORMAT_BASE));
    integer_part /= FLOAT_FORMAT_BASE;
    buffer[2] = '.';
    int last = 2;
    while (integer_part > 0) {
        last++;
        if (last >= buffer_size) {
            return FLOAT_FORMAT_ERR_OVERFLOW;
        }
        buffer[last] = (char)('0' + (integer_part % FLOAT_FORMAT_BASE));
        integer_part /= FLOAT_FORMAT_BASE;
    }
    if (buffer[last] == '.') {
        last++;
        if (last == buffer_size) {
            return FLOAT_FORMAT_ERR_OVERFLOW;
        }
        buffer[last] = '0';
    }
    if (sign == -1) {
        last++;
        if (last == buffer_size) {
            return FLOAT_FORMAT_ERR_OVERFLOW;
        }
        buffer[last] = '-';
    }
    for (int j = 0; j < last; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[last];
        buffer[last] = temp;
        last--;
    }

    return FLOAT_FORMAT_OK;
}