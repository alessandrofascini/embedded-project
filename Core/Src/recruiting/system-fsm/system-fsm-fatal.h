/*!
 * \file system-fsm-fatal.h
 * \author Alessandro Fascini
 * \brief Private fatal error handling of the system FSM, shared only inside the module.
 */

#ifndef RECRUITING_SYSTEM_FSM_FATAL_H
#define RECRUITING_SYSTEM_FSM_FATAL_H

#include "system-fsm.h"

void int_system_fsm_fatal_init(struct SystemFsmFatal *fatal);

void int_system_fsm_go_fatal(struct SystemFsm *self, enum SystemFsmFatalErrorCode code);

void int_system_fsm_fatal_report(struct SystemFsm *self);

#endif
