/*!
 * \file system-fsm-cli.h
 * \author Alessandro Fascini
 * \brief Private CLI gating of the system FSM, shared only inside the module.
 */

#ifndef RECRUITING_SYSTEM_FSM_CLI_H
#define RECRUITING_SYSTEM_FSM_CLI_H

#include "system-fsm.h"

void int_system_fsm_cli_enable(const struct SystemFsmCli *cli);

void int_system_fsm_cli_disable(void);

#endif
