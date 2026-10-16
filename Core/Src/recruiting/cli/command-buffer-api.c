/*!
 * \file
 * \author Alessandro Fascini
 *
 * \brief
 */

#include "command-buffer-api.h"

#define COMMAND_BUFFER_TERMINATOR_BACKSPACE ('\b')
#define COMMAND_BUFFER_TERMINATOR_CR ('\r')
#define COMMAND_BUFFER_TERMINATOR_LF ('\n')
#define COMMAND_BUFFER_TERMINATOR_END_OF_LINE ('\0')

enum CommandBufferStatus command_buffer_init(struct CommandBuffer *self, char *buffer, size_t capacity) {
    if (self == NULL) {
        return COMMAND_BUFFER_SELF_IS_NULL;
    }
    if (buffer == NULL) {
        return COMMAND_BUFFER_IS_NULL;
    }
    if (capacity == 0) {
        return COMMAND_BUFFER_NO_CAPACITY;
    }
    self->buffer = buffer;
    self->length = capacity - 1;
    self->buffer[self->length] = COMMAND_BUFFER_TERMINATOR_END_OF_LINE; // Ensure null-termination
    self->index = 0;
    self->buffer[0] = COMMAND_BUFFER_TERMINATOR_END_OF_LINE;
    return COMMAND_BUFFER_OK;
}

enum CommandBufferStatus command_buffer_clear(struct CommandBuffer *command_buffer) {
    if (command_buffer == NULL) {
        return COMMAND_BUFFER_SELF_IS_NULL;
    }
    command_buffer->index = 0;
    command_buffer->buffer[0] = COMMAND_BUFFER_TERMINATOR_END_OF_LINE;
    return COMMAND_BUFFER_OK;
}

enum CommandBufferStatus command_buffer_append(struct CommandBuffer *command_buffer, char c) {
    if (command_buffer == NULL) {
        return COMMAND_BUFFER_SELF_IS_NULL;
    }
    if (c == COMMAND_BUFFER_TERMINATOR_BACKSPACE) {
        if (command_buffer->index > 0) {
            command_buffer->buffer[--command_buffer->index] = COMMAND_BUFFER_TERMINATOR_END_OF_LINE;
        }
        return COMMAND_BUFFER_OK;
    }
    if (command_buffer->index >= command_buffer->length) {
        return COMMAND_BUFFER_FULL;
    }
    command_buffer->buffer[command_buffer->index] = c;
    command_buffer->buffer[++command_buffer->index] = COMMAND_BUFFER_TERMINATOR_END_OF_LINE;
    return COMMAND_BUFFER_OK;
}

bool command_buffer_has_terminator(struct CommandBuffer *command_buffer) {
    return command_buffer != NULL &&
           command_buffer->index > 0 &&
           (command_buffer->buffer[command_buffer->index - 1] == COMMAND_BUFFER_TERMINATOR_CR ||
            command_buffer->buffer[command_buffer->index - 1] == COMMAND_BUFFER_TERMINATOR_LF);
}

bool command_buffer_is_empty(struct CommandBuffer *command_buffer) {
    return command_buffer != NULL && command_buffer->index == 0;
}