/*!
 * \file
 * \author Alessandro Fascini
 *
 * \brief
 */

#include "cli-api.h"
#include "command-buffer-api.h"
#include "command-executor-api.h"

#include <assert.h>

void cli_init(struct Cli *self, struct CommandBuffer *buffer, struct CommandExecutor *executor) {
    if (self == NULL || buffer == NULL || executor == NULL) {
        /* TODO: Improve this */
        return;
    }
    self->status = CLI_FEED_EMPTY;
    self->buffer = *buffer;
    self->executor = *executor;
}

enum CliFeedStatus cli_feed(struct Cli *self, char byte) {
    if (self == NULL) {
        return CLI_FEED_SELF_IS_NULL;
    }
    const enum CommandBufferStatus status = command_buffer_append(&self->buffer, byte);
    if (status != COMMAND_BUFFER_OK) {
        /* TODO: passthrough buffer errors */
        self->status = CLI_FEED_NOT_CHANGED;
        return self->status;
    }
    if (command_buffer_has_terminator(&self->buffer)) {
        self->status = CLI_FEED_EXECUTABLE;
    } else {
        self->status = CLI_FEED_UPDATED;
    }
    return self->status;
}

enum CommandExecutorStatus cli_execute(struct Cli *self) {
    if (self == NULL || self->status != CLI_FEED_EXECUTABLE) {
        return COMMAND_EXECUTOR_SELF_IS_NULL;
    }
    const enum CommandExecutorStatus exec_status = command_executor_execute(&self->executor, self->buffer.buffer);

    const enum CommandBufferStatus buff_status = command_buffer_clear(&self->buffer);
    assert(buff_status == COMMAND_BUFFER_OK);

    self->status = CLI_FEED_EMPTY;

    return exec_status;
}

char *cli_get_command(struct Cli *self) {
    if (self == NULL) {
        return NULL;
    }
    return self->buffer.buffer;
}

void cli_reset(struct Cli *self) {
    if (self == NULL) {
        return;
    }
    const enum CommandBufferStatus buff_status = command_buffer_clear(&self->buffer);
    assert(buff_status == COMMAND_BUFFER_OK);
    self->status = CLI_FEED_EMPTY;
}

void cli_set_status_unchanged(struct Cli *self) {
    if (self == NULL) {
        return;
    }
    self->status = CLI_FEED_NOT_CHANGED;
}