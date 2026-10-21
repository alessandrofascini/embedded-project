/*!
 * \file system-fsm-fatal.h
 * \author Alessandro Fascini
 * \brief Private fatal error handling of the system FSM, shared only inside the module.
 */

#ifndef RECRUITING_SYSTEM_FSM_FATAL_H
#define RECRUITING_SYSTEM_FSM_FATAL_H

#include "system-fsm.h"

/*!
 * \brief Clear the fatal state data: no error recorded yet.
 * \param fatal The fatal state data; must not be NULL.
 */
void int_system_fsm_fatal_init(struct SystemFsmFatal *fatal);

/*!
 * \brief Record an error and request the transition to the fatal state.
 *
 * The transition happens at the end of the current system_fsm_run(). Only
 * the first error is kept: it is the root cause, later ones are consequences.
 *
 * \param self The SystemFsm instance; must not be NULL.
 * \param code The reason for going fatal.
 */
void int_system_fsm_go_fatal(struct SystemFsm *self, enum SystemFsmFatalErrorCode code);

/*!
 * \brief Print the recorded error, at most once every 100 ms.
 *
 * Called on every run of the fatal state.
 *
 * \param self The SystemFsm instance; must not be NULL.
 */
void int_system_fsm_fatal_report(struct SystemFsm *self);

#endif
