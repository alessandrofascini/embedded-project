#ifndef RECRUITING_COMMAND_EXECUTOR_API_H
#define RECRUITING_COMMAND_EXECUTOR_API_H

#include "command-executor.h"

enum CommandExecutorStatus command_executor_init(struct CommandExecutor *self, const struct Command *commands, size_t command_count);

enum CommandExecutorStatus command_executor_execute(const struct CommandExecutor *self, char *line);

#endif
