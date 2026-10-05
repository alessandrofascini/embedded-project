/*!
 * \file float-format-api.c
 * \author Alessandro Fascini
 *
 * \brief Implementation of a simple float formatter.
 */
#include "float-format-api.h"
#include <math.h>

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
    int integer_part = (int)(value * 10);
    buffer[0] = '\0';
    buffer[1] = '0' + (integer_part % 10);
    integer_part /= 10;
    buffer[2] = '.';
    int i = 2;
    while (integer_part > 0) {
        i++;
        if (i >= buffer_size) {
            return FLOAT_FORMAT_ERR_OVERFLOW;
        }
        buffer[i] = '0' + (integer_part % 10);
        integer_part /= 10;
    }
    if (buffer[i] == '.') {
        i++;
        if (i == buffer_size) {
            return FLOAT_FORMAT_ERR_OVERFLOW;
        }
        buffer[i] = '0';
    }
    if (sign == -1) {
        i++;
        if (i == buffer_size) {
            return FLOAT_FORMAT_ERR_OVERFLOW;
        }
        buffer[i] = '-';
    }
    for (int j = 0; j < i; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i];
        buffer[i] = temp;
        i--;
    }

    return FLOAT_FORMAT_OK;
}