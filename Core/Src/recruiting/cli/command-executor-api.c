/*!
 * \file command-executor-api.c
 * \author Alessandro Fascini
 * \brief Implementation of an in-place line tokenizer and table-based command dispatcher.
 */

#include "command-executor-api.h"

#include <stdbool.h>
#include <string.h>
#include <assert.h>

static bool prv_is_separator(char c);

enum CommandExecutorStatus command_executor_init(struct CommandExecutor *self, const struct Command *commands, size_t command_count) {
    if (self == NULL) {
        return COMMAND_EXECUTOR_SELF_IS_NULL;
    }
    if (commands == NULL) {
        return COMMAND_EXECUTOR_COMMANDS_IS_NULL;
    }
    for (size_t i = 0; i < command_count; i++) {
        if (commands[i].grammar == NULL || commands[i].handler == NULL) {
            return COMMAND_EXECUTOR_INVALID_COMMAND;
        }
    }
    self->commands = commands;
    self->command_count = command_count;
    return COMMAND_EXECUTOR_OK;
}

enum CommandExecutorStatus command_executor_execute(const struct CommandExecutor *self, char *line) {
    if (self == NULL) {
        return COMMAND_EXECUTOR_SELF_IS_NULL;
    }
    if (line == NULL) {
        return COMMAND_EXECUTOR_LINE_IS_NULL;
    }

    char *argv[COMMAND_EXECUTOR_MAX_ARGS];
    int argc = 0;
    char *cursor = line;

    while (true) {
        while (prv_is_separator(*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        if (argc == COMMAND_EXECUTOR_MAX_ARGS) {
            return COMMAND_EXECUTOR_TOO_MANY_ARGS;
        }
        argv[argc++] = cursor;
        while (*cursor != '\0' && !prv_is_separator(*cursor)) {
            cursor++;
        }
        /* Only step past a separator: stepping past '\0' would read beyond the line. */
        if (*cursor != '\0') {
            *cursor = '\0';
            cursor++;
        }
    }

    if (argc == 0) {
        return COMMAND_EXECUTOR_EMPTY_LINE;
    }

    /* TODO: use TRIE */
    for (size_t i = 0; i < self->command_count; i++) {
        if (strcmp(argv[0], self->commands[i].grammar) == 0) {
            const enum CommandHandlerStatus status = self->commands[i].handler(argc, argv);
            return status == COMMAND_HANDLER_OK ? COMMAND_EXECUTOR_OK : COMMAND_EXECUTOR_INVALID_ARGS;
        }
    }
    return COMMAND_EXECUTOR_COMMAND_NOT_FOUND;
}

/*!
 * \brief Check if a character separates two tokens (space, tab, CR or LF).
 * \param c The character to check.
 * \return true if the character is a separator, false otherwise.
 */
static bool prv_is_separator(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}