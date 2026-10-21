/*!
 * \file system-fsm-cli.h
 * \author Alessandro Fascini
 * \brief Private CLI gating of the system FSM, shared only inside the module.
 */

#ifndef RECRUITING_SYSTEM_FSM_CLI_H
#define RECRUITING_SYSTEM_FSM_CLI_H

#include "system-fsm.h"

/*!
 * \brief Clear the command line, then let the CLI accept input.
 *
 * \attention Call it only while the CLI is disabled: the line is then reset
 * before the UART ISR may feed it, so the ISR never writes into a buffer
 * that is being cleared.
 *
 * \param cli The CLI callbacks; must not be NULL.
 */
void int_system_fsm_cli_enable(const struct SystemFsmCli *cli);

/*!
 * \brief Stop the CLI from accepting input.
 */
void int_system_fsm_cli_disable(void);

#endif
