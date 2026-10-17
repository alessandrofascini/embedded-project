#ifndef RECRUITING_COMMAND_EXECUTOR_API_H
#define RECRUITING_COMMAND_EXECUTOR_API_H

#include "command-executor.h"

/*!
 * \brief Initialize a command executor over a caller-provided command table.
 * \param self          The CommandExecutor instance to initialize.
 * \param commands      Dispatch table; not copied, must outlive self.
 *                      Every entry needs a non-NULL grammar and handler.
 * \param command_count Number of entries in commands (0 is allowed: every command is then not found).
 * \return COMMAND_EXECUTOR_OK, COMMAND_EXECUTOR_SELF_IS_NULL,
 *         COMMAND_EXECUTOR_COMMANDS_IS_NULL or COMMAND_EXECUTOR_INVALID_COMMAND.
 */
enum CommandExecutorStatus command_executor_init(struct CommandExecutor *self, const struct Command *commands, size_t command_count);

/*!
 * \brief Split a line into tokens and call the handler whose grammar equals the first one.
 *
 * Tokens are separated by spaces, tabs, CR and LF; the line is modified in
 * place (the first separator after each token is overwritten with NUL).
 *
 * \param self The CommandExecutor instance.
 * \param line The NUL-terminated line to execute.
 * \return COMMAND_EXECUTOR_OK if the handler succeeded, COMMAND_EXECUTOR_INVALID_ARGS
 *         if it rejected its arguments, or COMMAND_EXECUTOR_SELF_IS_NULL,
 *         COMMAND_EXECUTOR_LINE_IS_NULL, COMMAND_EXECUTOR_EMPTY_LINE,
 *         COMMAND_EXECUTOR_TOO_MANY_ARGS or COMMAND_EXECUTOR_COMMAND_NOT_FOUND.
 */
enum CommandExecutorStatus command_executor_execute(const struct CommandExecutor *self, char *line);

#endif
