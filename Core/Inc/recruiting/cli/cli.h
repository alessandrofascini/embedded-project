#ifndef RECRUITING_CLI_H
#define RECRUITING_CLI_H

#include <stddef.h>

#include "command-buffer.h"
#include "command-executor.h"

enum CliFeedStatus {
    CLI_FEED_EMPTY,
    CLI_FEED_NOT_CHANGED,
    CLI_FEED_UPDATED,
    CLI_FEED_EXECUTABLE,
    CLI_FEED_SELF_IS_NULL,
};

struct Cli {
    volatile enum CliFeedStatus status;
    struct CommandBuffer buffer;
    struct CommandExecutor executor;
};

#endif