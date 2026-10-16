#ifndef RECRUITING_COMMAND_EXECUTOR_H
#define RECRUITING_COMMAND_EXECUTOR_H

#include <stddef.h>

#define COMMAND_EXECUTOR_MAX_ARGS (4)

enum CommandHandlerStatus {
    COMMAND_HANDLER_OK = 0,            /*!< Arguments accepted and command performed. */
    COMMAND_HANDLER_INVALID_ARGS = -1, /*!< Arguments missing, extra or malformed. */
};

typedef enum CommandHandlerStatus (*command_handler_fn)(int argc, char *argv[]);

struct Command {
    char *grammar;              /*!< Command name, matched against argv[0] as a whole token. */
    command_handler_fn handler; /*!< Function called when the name matches. */
};

enum CommandExecutorStatus {
    COMMAND_EXECUTOR_OK = 0,                 /*!< Success. */
    COMMAND_EXECUTOR_SELF_IS_NULL = -1,      /*!< self is NULL. */
    COMMAND_EXECUTOR_COMMANDS_IS_NULL = -2,  /*!< commands is NULL. */
    COMMAND_EXECUTOR_INVALID_COMMAND = -3,   /*!< A table entry has a NULL grammar or handler. */
    COMMAND_EXECUTOR_LINE_IS_NULL = -4,      /*!< line is NULL. */
    COMMAND_EXECUTOR_EMPTY_LINE = -5,        /*!< line contains only separators. */
    COMMAND_EXECUTOR_TOO_MANY_ARGS = -6,     /*!< line has more than COMMAND_EXECUTOR_MAX_ARGS tokens. */
    COMMAND_EXECUTOR_COMMAND_NOT_FOUND = -7, /*!< argv[0] matches no table entry. */
    COMMAND_EXECUTOR_INVALID_ARGS = -8,      /*!< The handler returned COMMAND_HANDLER_INVALID_ARGS. */
};

struct CommandExecutor {
    const struct Command *commands; /*!< Caller-owned dispatch table, not copied; must outlive self. */
    size_t command_count;           /*!< Number of entries in commands. */
};

#endif
