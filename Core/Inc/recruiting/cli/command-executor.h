/*!
 * \file command-executor.h
 * \author Alessandro Fascini
 * \brief Types of the command executor: command table entries, handler signature and status codes.
 */

#ifndef RECRUITING_COMMAND_EXECUTOR_H
#define RECRUITING_COMMAND_EXECUTOR_H

#include <stddef.h>

/*!
 * \brief Maximum number of tokens in a line, command name included.
 */
#define COMMAND_EXECUTOR_MAX_ARGS (4)

/*!
 * \brief Result returned by a command handler.
 */
enum CommandHandlerStatus {
    COMMAND_HANDLER_OK,           /*!< Arguments accepted and command performed. */
    COMMAND_HANDLER_INVALID_ARGS, /*!< Arguments missing, extra or malformed. */
};

/*!
 * \brief Function implementing a command.
 *
 * argv[0] is the command name itself. The argv strings point into the line
 * being executed and are only valid for the duration of the call.
 * A handler returning COMMAND_HANDLER_INVALID_ARGS is responsible for
 * reporting the problem to the user: the executor never prints anything.
 *
 * \param argc Number of tokens in argv (at least 1).
 * \param argv The tokens of the line; not NULL-terminated, use argc.
 * \return COMMAND_HANDLER_OK, or COMMAND_HANDLER_INVALID_ARGS
 *         (any value other than COMMAND_HANDLER_OK is treated as invalid arguments).
 */
typedef enum CommandHandlerStatus (*command_handler_fn)(int argc, char *argv[]);

/*!
 * \brief Entry of a command table: a command name and its handler.
 */
struct Command {
    char *grammar;              /*!< Command name, matched against argv[0] as a whole token. */
    command_handler_fn handler; /*!< Function called when the name matches. */
};

/*!
 * \brief Result codes for the command executor functions.
 */
enum CommandExecutorStatus {
    COMMAND_EXECUTOR_OK,                /*!< Success. */
    COMMAND_EXECUTOR_SELF_IS_NULL,      /*!< self is NULL. */
    COMMAND_EXECUTOR_COMMANDS_IS_NULL,  /*!< commands is NULL. */
    COMMAND_EXECUTOR_INVALID_COMMAND,   /*!< A table entry has a NULL grammar or handler. */
    COMMAND_EXECUTOR_LINE_IS_NULL,      /*!< line is NULL. */
    COMMAND_EXECUTOR_EMPTY_LINE,        /*!< line contains only separators. */
    COMMAND_EXECUTOR_TOO_MANY_ARGS,     /*!< line has more than COMMAND_EXECUTOR_MAX_ARGS tokens. */
    COMMAND_EXECUTOR_COMMAND_NOT_FOUND, /*!< argv[0] matches no table entry. */
    COMMAND_EXECUTOR_INVALID_ARGS,      /*!< The handler returned anything other than COMMAND_HANDLER_OK. */
    COMMAND_EXECUTOR_LINE_NOT_READY,    /*!< The line is not complete yet (only returned by cli_execute()). */
};

/*!
 * \brief Splits a line into tokens and dispatches it to the matching command.
 */
struct CommandExecutor {
    const struct Command *commands; /*!< Caller-owned dispatch table, not copied; must outlive self. */
    size_t command_count;           /*!< Number of entries in commands. */
};

#endif
