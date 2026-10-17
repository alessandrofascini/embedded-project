#ifndef RECRUITING_CLI_H
#define RECRUITING_CLI_H

#include <stddef.h>

#include "command-buffer.h"
#include "command-executor.h"

/*!
 * \brief Result codes for cli_init().
 */
enum CliInitStatus {
    CLI_INIT_OK = 0,           /*!< Initialization succeeded. */
    CLI_INIT_SELF_IS_NULL,     /*!< self is NULL. */
    CLI_INIT_STORAGE_IS_NULL,  /*!< storage is NULL. */
    CLI_INIT_NO_CAPACITY,      /*!< storage_size is 0. */
    CLI_INIT_COMMANDS_IS_NULL, /*!< commands is NULL. */
    CLI_INIT_INVALID_COMMAND,  /*!< A command table entry has a NULL grammar or handler. */
};

/*!
 * \brief State of the line being typed, as seen by whoever renders the CLI.
 *
 * Set by cli_feed() (usually from the UART RX interrupt) and by the other
 * cli_* functions, which may also run in interrupt context.
 */
enum CliFeedStatus {
    CLI_FEED_EMPTY,        /*!< The line was just cleared (init, execute, reset): a new prompt should be shown. */
    CLI_FEED_NOT_CHANGED,  /*!< Nothing new to show since the last acknowledgement, or the last byte was rejected (line full). */
    CLI_FEED_UPDATED,      /*!< A character was added or removed: the line should be redrawn. */
    CLI_FEED_EXECUTABLE,   /*!< The line ends with CR or LF and is ready for cli_execute(). */
    CLI_FEED_SELF_IS_NULL, /*!< Returned by cli_feed() when self is NULL; never stored in Cli.status. */
};

/*!
 * \brief Line-based command interpreter: collects received bytes into a line
 *        and dispatches complete lines to a command table.
 */
struct Cli {
    volatile enum CliFeedStatus status; /*!< Current line state; volatile because it is written from interrupt context. */
    struct CommandBuffer buffer;        /*!< Line being typed. */
    struct CommandExecutor executor;    /*!< Tokenizer and dispatcher for complete lines. */
};

#endif
