#ifndef RECRUITING_COMMAND_BUFFER_API_H
#define RECRUITING_COMMAND_BUFFER_API_H

#include "command-buffer.h"

enum CommandBufferStatus command_buffer_init(struct CommandBuffer *self, char *buffer, size_t capacity);

enum CommandBufferStatus command_buffer_clear(struct CommandBuffer *command_buffer);

enum CommandBufferStatus command_buffer_append(struct CommandBuffer *command_buffer, char c);

bool command_buffer_has_terminator(struct CommandBuffer *command_buffer);

bool command_buffer_is_empty(struct CommandBuffer *command_buffer);

#endif