/*!
 * \file cli-api.c
 * \author Alessandro Fascini
 * \brief Implementation of the CLI facade: line input and command dispatch.
 *
 * The CLI is split in three parts: a CommandBuffer that collects the bytes of
 * the line being typed, a CommandExecutor that tokenizes a complete line and
 * calls the matching handler, and this facade that glues them together and
 * tracks the line state (enum CliFeedStatus) for whoever renders the prompt.
 * The modules never perform I/O themselves: receiving bytes and printing are
 * left to the caller, which keeps them hardware-agnostic and testable on the host.
 */

#include "cli-api.h"
#include "command-buffer-api.h"
#include "command-executor-api.h"

#include <assert.h>

enum CliInitStatus cli_init(struct Cli *self, char *storage, size_t storage_size, const struct Command *commands, size_t command_count) {
    if (self == NULL) {
        return CLI_INIT_SELF_IS_NULL;
    }
    const enum CommandBufferStatus buffer_status = command_buffer_init(&self->buffer, storage, storage_size);
    if (buffer_status == COMMAND_BUFFER_IS_NULL) {
        return CLI_INIT_STORAGE_IS_NULL;
    }
    if (buffer_status == COMMAND_BUFFER_NO_CAPACITY) {
        return CLI_INIT_NO_CAPACITY;
    }
    /* COMMAND_BUFFER_SELF_IS_NULL is unreachable: &self->buffer is never NULL. */
    assert(buffer_status == COMMAND_BUFFER_OK);

    const enum CommandExecutorStatus executor_status = command_executor_init(&self->executor, commands, command_count);
    if (executor_status == COMMAND_EXECUTOR_COMMANDS_IS_NULL) {
        return CLI_INIT_COMMANDS_IS_NULL;
    }
    if (executor_status == COMMAND_EXECUTOR_INVALID_COMMAND) {
        return CLI_INIT_INVALID_COMMAND;
    }
    /* COMMAND_EXECUTOR_SELF_IS_NULL is unreachable: &self->executor is never NULL. */
    assert(executor_status == COMMAND_EXECUTOR_OK);

    self->status = CLI_FEED_EMPTY;
    return CLI_INIT_OK;
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
    if (self == NULL || self->status == CLI_FEED_EXECUTABLE) {
        return;
    }
    self->status = CLI_FEED_NOT_CHANGED;
}