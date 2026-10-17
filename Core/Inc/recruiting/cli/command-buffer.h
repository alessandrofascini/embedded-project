/*!
 * \file command-buffer.h
 * \author Alessandro Fascini
 * \brief Types of the command buffer: the CommandBuffer instance and its status codes.
 */

#ifndef RECRUITING_COMMAND_BUFFER_H
#define RECRUITING_COMMAND_BUFFER_H

#include <stdint.h>
#include <stddef.h>

/*!
 * \brief Result codes for the command buffer functions.
 */
enum CommandBufferStatus {
    COMMAND_BUFFER_OK = 0,       /*!< Operation succeeded. */
    COMMAND_BUFFER_FULL,         /*!< The line already holds its maximum number of characters. */
    COMMAND_BUFFER_OVERFLOW,     /*!< Reserved: currently never returned. */
    COMMAND_BUFFER_SELF_IS_NULL, /*!< self is NULL. */
    COMMAND_BUFFER_IS_NULL,      /*!< The storage passed to command_buffer_init() is NULL. */
    COMMAND_BUFFER_NO_CAPACITY,  /*!< The capacity passed to command_buffer_init() is 0. */
};

/*!
 * \brief Fixed-size, always NUL-terminated line buffer over caller-owned storage.
 */
struct CommandBuffer {
    char *buffer;   /*!< Caller-owned storage, not copied; always NUL-terminated. */
    size_t length;  /*!< Maximum number of characters in the line (capacity - 1). */
    uint16_t index; /*!< Number of characters currently stored, i.e. the position of the NUL. */
};

#endif
