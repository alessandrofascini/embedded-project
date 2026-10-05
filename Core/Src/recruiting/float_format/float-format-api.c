/*!
 * \file float-format-api.c
 * \author Alessandro Fascini
 *
 * \brief Implementation of a simple float formatter.
 */
#include "float-format-api.h"
#include <math.h>

int float_to_string(float value, char *buffer, uint16_t buffer_size) {
    if (buffer == NULL || buffer_size == 0 || buffer_size < 2 || isnan(value)) {
        return 0;
    }
    int integer_part = (int)(value * 10);
    if (integer_part == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return -1;
    }
    if (buffer_size < 3) {
        return 1;
    }
    buffer[0] = '\0';
    int reminder = integer_part % 10;
    buffer[1] = '0' + reminder;
    integer_part /= 10;
    buffer[2] = '.';
    int i = 3;
    while (integer_part > 0) {
        if (i >= buffer_size) {
            return 2;
        }
        buffer[i] = '0' + (integer_part % 10);
        integer_part /= 10;
        i++;
    }
    i--;
    for (int j = 0; j < i; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i];
        buffer[i] = temp;
        i--;
    }
    return -1;
}