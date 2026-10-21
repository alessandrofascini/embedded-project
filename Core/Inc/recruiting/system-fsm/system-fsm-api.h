/*!
 * \file system-fsm-api.h
 * \author Alessandro Fascini
 * \brief Public API of the system FSM.
 */

#ifndef RECRUITING_SYSTEM_FSM_API_H
#define RECRUITING_SYSTEM_FSM_API_H

#include <stdbool.h>
#include "system-fsm.h"

enum SystemFsmStatus system_fsm_init(struct SystemFsm *self);

enum SystemFsmStatus system_fsm_run(struct SystemFsm *self);

enum SystemFsmState system_fsm_get_state(const struct SystemFsm *self);

bool system_fsm_is_cli_enabled(const struct SystemFsm *self);

void system_fsm_post_trigger(struct SystemFsm *self, enum SystemFsmTrigger trigger);

#endif
