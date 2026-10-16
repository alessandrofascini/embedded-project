#ifndef RECRUITING_CLI_API_H
#define RECRUITING_CLI_API_H

#include "cli.h"

void cli_init(struct Cli *self, struct CommandBuffer *buffer, struct CommandExecutor *executor);

enum CliFeedStatus cli_feed(struct Cli *self, char byte);

enum CommandExecutorStatus cli_execute(struct Cli *self);

char *cli_get_command(struct Cli *self);

void cli_reset(struct Cli *self);

void cli_set_status_unchanged(struct Cli *self);

#endif