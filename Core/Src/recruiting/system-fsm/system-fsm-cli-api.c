/*!
 * \file system-fsm-cli-api.c
 * \author Alessandro Fascini
 * \brief CLI gating of the system FSM: the only owner of the CLI status flag.
 */

#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include "system-fsm-api.h"
#include "system-fsm-cli.h"

#define SYSTEM_FSM_CLI_STATUS_ON (true)
#define SYSTEM_FSM_CLI_STATUS_OFF (false)

/* read by the UART ISR through system_fsm_is_cli_enabled() */
static volatile bool system_fsm_cli_status = SYSTEM_FSM_CLI_STATUS_OFF;

bool system_fsm_is_cli_enabled(void) {
    return system_fsm_cli_status;
}

void int_system_fsm_cli_enable(const struct SystemFsmCli *cli) {
    assert(cli != NULL);
    /* reset first: while the status is still off the UART ISR does not touch the buffer */
    cli->reset();
    system_fsm_cli_status = SYSTEM_FSM_CLI_STATUS_ON;
}

void int_system_fsm_cli_disable(void) {
    system_fsm_cli_status = SYSTEM_FSM_CLI_STATUS_OFF;
}
