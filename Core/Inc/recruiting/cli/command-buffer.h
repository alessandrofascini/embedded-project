#ifndef RECRUITING_COMMAND_BUFFER_H
#define RECRUITING_COMMAND_BUFFER_H

#include <stdint.h>
#include <stddef.h>

enum CommandBufferStatus {
    COMMAND_BUFFER_OK = 0,
    COMMAND_BUFFER_FULL,
    COMMAND_BUFFER_OVERFLOW,
    COMMAND_BUFFER_SELF_IS_NULL,
    COMMAND_BUFFER_IS_NULL,
    COMMAND_BUFFER_NO_CAPACITY,
};

struct CommandBuffer {
    char *buffer;
    size_t length;
    uint16_t index;
};

#endif