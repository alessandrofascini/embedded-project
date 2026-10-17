/*!
 * \file command-buffer-api.h
 * \author Alessandro Fascini
 * \brief API of the command buffer: a fixed-size line with backspace support.
 */

#ifndef RECRUITING_COMMAND_BUFFER_API_H
#define RECRUITING_COMMAND_BUFFER_API_H

#include "command-buffer.h"

/*!
 * \brief Initialize an empty command buffer over caller-provided storage.
 * \param self     The CommandBuffer instance to initialize.
 * \param buffer   Storage for the line; not copied, must outlive self.
 * \param capacity Size of buffer in bytes, including the terminating NUL.
 * \return COMMAND_BUFFER_OK, COMMAND_BUFFER_SELF_IS_NULL, COMMAND_BUFFER_IS_NULL
 *         or COMMAND_BUFFER_NO_CAPACITY.
 */
enum CommandBufferStatus command_buffer_init(struct CommandBuffer *self, char *buffer, size_t capacity);

/*!
 * \brief Empty the line.
 * \param command_buffer The CommandBuffer instance.
 * \return COMMAND_BUFFER_OK, or COMMAND_BUFFER_SELF_IS_NULL if command_buffer is NULL.
 */
enum CommandBufferStatus command_buffer_clear(struct CommandBuffer *command_buffer);

/*!
 * \brief Append a character to the line, keeping it NUL-terminated.
 *
 * A backspace ('\b') removes the last character instead of being stored;
 * on an empty line it does nothing.
 *
 * \param command_buffer The CommandBuffer instance.
 * \param c              The character to append.
 * \return COMMAND_BUFFER_OK, COMMAND_BUFFER_FULL if the line is already full
 *         (c is dropped), or COMMAND_BUFFER_SELF_IS_NULL.
 */
enum CommandBufferStatus command_buffer_append(struct CommandBuffer *command_buffer, char c);

/*!
 * \brief Check whether the line ends with a line terminator (CR or LF).
 * \param command_buffer The CommandBuffer instance.
 * \return true if the last character is '\r' or '\n'; false if it is not,
 *         if the line is empty or if command_buffer is NULL.
 */
bool command_buffer_has_terminator(struct CommandBuffer *command_buffer);

/*!
 * \brief Check whether the line is empty.
 * \param command_buffer The CommandBuffer instance.
 * \return true if the line holds no characters; false otherwise or if command_buffer is NULL.
 */
bool command_buffer_is_empty(struct CommandBuffer *command_buffer);

#endif
