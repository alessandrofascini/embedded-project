/*!
 * \file cli-api.h
 * \author Alessandro Fascini
 * \brief Public API of the CLI: initialization, byte input and command execution.
 */

#ifndef RECRUITING_CLI_API_H
#define RECRUITING_CLI_API_H

#include "cli.h"

/*!
 * \brief Initialize a CLI over caller-provided line storage and command table.
 *
 * Neither storage nor commands is copied: both are owned by the caller and
 * must outlive self. On error self is only partially initialized and must
 * not be used.
 *
 * \param self          The Cli instance to initialize.
 * \param storage       Buffer holding the line being typed.
 * \param storage_size  Size of storage in bytes, including the terminating NUL
 *                      (the longest line is storage_size - 1 characters).
 * \param commands      Dispatch table; every entry needs a non-NULL grammar and handler.
 * \param command_count Number of entries in commands.
 * \return CLI_INIT_OK on success, or the enum CliInitStatus value describing the first invalid argument.
 */
enum CliInitStatus cli_init(struct Cli *self, char *storage, size_t storage_size, const struct Command *commands, size_t command_count);

/*!
 * \brief Add one received byte to the current line.
 *
 * Meant to be called from the UART RX interrupt, one byte at a time.
 * A backspace ('\b') removes the last character instead of being stored.
 *
 * While a line is ready (self->status is CLI_FEED_EXECUTABLE), every byte is
 * dropped until cli_execute() or cli_reset() empties the line: the line
 * cannot change while it is waiting for, or under, execution.
 *
 * \param self The Cli instance.
 * \param byte The received byte.
 * \return The new status, also stored in self->status:
 *         CLI_FEED_EXECUTABLE if a line was already ready (byte dropped), or if,
 *         after handling byte, the line ends with CR or LF,
 *         CLI_FEED_UPDATED if byte was handled and the line does not end with CR or LF
 *         (also returned for a backspace on an empty line, which changes nothing),
 *         CLI_FEED_NOT_CHANGED if the line is full and byte was dropped (CR and LF included),
 *         CLI_FEED_SELF_IS_NULL if self is NULL.
 */
enum CliFeedStatus cli_feed(struct Cli *self, char byte);

/*!
 * \brief Run the current line through the command table, then clear it.
 *
 * Only does something when self->status is CLI_FEED_EXECUTABLE; afterwards
 * the line is empty and the status is CLI_FEED_EMPTY.
 *
 * \param self The Cli instance.
 * \return The enum CommandExecutorStatus of the execution, or
 *         COMMAND_EXECUTOR_SELF_IS_NULL if self is NULL or no line is ready.
 */
enum CommandExecutorStatus cli_execute(struct Cli *self);

/*!
 * \brief Get the line typed so far, e.g. to echo it back to the terminal.
 * \param self The Cli instance.
 * \return The NUL-terminated line (the storage passed to cli_init(), not a copy:
 *         its content changes with every cli_feed(), cli_execute() or cli_reset());
 *         NULL if self is NULL.
 */
char *cli_get_command(struct Cli *self);

/*!
 * \brief Discard the current line and set the status to CLI_FEED_EMPTY.
 * \param self The Cli instance; nothing happens if NULL.
 */
void cli_reset(struct Cli *self);

/*!
 * \brief Acknowledge the current status once it has been rendered
 *        (prompt shown or line redrawn), setting it to CLI_FEED_NOT_CHANGED.
 *
 * A CLI_FEED_EXECUTABLE status is left untouched, so a ready line can only
 * be consumed by cli_execute() or discarded by cli_reset().
 *
 * \param self The Cli instance; nothing happens if NULL.
 */
void cli_set_status_unchanged(struct Cli *self);

#endif
