/*!
 * \file serial-print-api.c
 * \author Alessandro Fascini
 * \brief Implementation of the minimal formatted printer.
 *
 * \details Floats are not supported on purpose: a float passed through the
 * variadic arguments is promoted to double, which would link the software
 * double-precision routines (about 1-2 KB of flash on a core without an FPU).
 * Format them with float_to_string() into a buffer and print it with %s.
 */

#include <stdarg.h>
#include <stddef.h>

#include "serial-print-api.h"

#define SERIAL_PRINT_BASE (10U)
#define SERIAL_PRINT_MAX_DIGITS (10U) /* digits of UINT32_MAX */

static serial_print_write_char serial_print_write = NULL;

static void prv_put_string(const char *string);
static void prv_put_unsigned(unsigned int value);
static inline void prv_put_signed(int value);

void serial_print_init(serial_print_write_char write_char) {
    serial_print_write = write_char;
}

void serial_print(const char *format, ...) {
    if (serial_print_write == NULL || format == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    for (const char *cursor = format; *cursor != '\0'; cursor++) {
        if (*cursor != '%') {
            serial_print_write(*cursor);
            continue;
        }
        cursor++;
        switch (*cursor) {
            case 's':
                prv_put_string(va_arg(args, const char *));
                break;
            case 'd':
                prv_put_signed(va_arg(args, int));
                break;
            case 'u':
                prv_put_unsigned(va_arg(args, unsigned int));
                break;
            case 'c':
                serial_print_write((char)va_arg(args, int));
                break;
            case '%':
                serial_print_write('%');
                break;
            case '\0':
                /* a lone '%' at the end: stop before reading past the terminator */
                va_end(args);
                return;
            default:
                /* unsupported specifier: print it as is, so the mistake is visible */
                serial_print_write('%');
                serial_print_write(*cursor);
                break;
        }
    }
    va_end(args);
}

static void prv_put_string(const char *string) {
    if (string == NULL) {
        string = "(null)";
    }
    for (; *string != '\0'; string++) {
        serial_print_write(*string);
    }
}

static void prv_put_unsigned(unsigned int value) {
    char digits[SERIAL_PRINT_MAX_DIGITS];
    size_t count = 0;
    /* digits come out least significant first: collect them, then write them reversed */
    do {
        digits[count] = (char)('0' + (value % SERIAL_PRINT_BASE));
        count++;
        value /= SERIAL_PRINT_BASE;
    } while (value > 0U);
    while (count > 0U) {
        count--;
        serial_print_write(digits[count]);
    }
}

static inline void prv_put_signed(int value) {
    if (value >= 0) {
        prv_put_unsigned((unsigned int)value);
        return;
    }
    serial_print_write('-');
    /* negate in unsigned arithmetic: -INT_MIN overflows an int */
    prv_put_unsigned(0U - (unsigned int)value);
}
