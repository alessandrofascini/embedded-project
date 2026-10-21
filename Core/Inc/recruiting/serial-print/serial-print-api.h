/*!
 * \file serial-print-api.h
 * \author Alessandro Fascini
 * \brief API of a minimal formatted printer, a small replacement for printf.
 */

#ifndef RECRUITING_SERIAL_PRINT_API_H
#define RECRUITING_SERIAL_PRINT_API_H

#include "serial-print.h"

void serial_print_init(serial_print_write_char write_char);

/* supports only %s, %d, %u, %c and %%: the format attribute keeps the compiler checking the arguments */
void serial_print(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
