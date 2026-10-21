/*!
 * \file system-fsm-api.h
 * \author Alessandro Fascini
 * \brief Public API of the system FSM.
 *
 * \details The FSM starts in POST, runs the self-test and goes to WAITING, or
 * to FATAL on the first failure. From there:
 * - WAITING: a click or the run command goes to LISTENING.
 * - LISTENING: a click goes to PAUSED; NTC - POT above 5 °C for 5 s in a row
 *   goes to WARNING.
 * - PAUSED: a click or the run command goes back to LISTENING.
 * - WARNING: a 2 s button hold goes to WAITING; after 10 s it goes to FATAL.
 * - FATAL: a click resets the MCU, the only way out.
 *
 * The CLI is enabled only in WAITING and PAUSED. Triggers that a state does
 * not handle are ignored.
 */

#ifndef RECRUITING_SYSTEM_FSM_API_H
#define RECRUITING_SYSTEM_FSM_API_H

#include <stdbool.h>
#include "system-fsm.h"

/*!
 * \brief Initialize the FSM in the POST state with the CLI disabled.
 *
 * The callbacks must already be set in self. The sensor filters and models
 * are set up from config, which is not kept. The self-test runs on the first
 * call to system_fsm_run(), not here.
 *
 * \attention The callbacks inside self->cli and self->leds are only checked
 * with assert.
 *
 * \param self   The SystemFsm instance, with all its callbacks set.
 * \param config Board parameters of the sensors; only read here, so it may be a temporary.
 * \return SYSTEM_FSM_OK, SYSTEM_FSM_SELF_IS_NULL, one of the *_IS_NULL codes
 *         for the first missing callback, SYSTEM_FSM_CONFIG_IS_NULL, one of
 *         the *_INIT_FAILED ranges if a filter or model rejects its setup,
 *         or SYSTEM_FSM_INVALID_MACHINE.
 */
enum SystemFsmStatus system_fsm_init(struct SystemFsm *self, const struct SystemFsmConfig *config);

/*!
 * \brief Run one step of the current state, then apply a requested transition; call it from the main loop.
 *
 * The state routine runs once. If it, a trigger or a fatal error requested a
 * transition, the enter function of the new state runs and the state switches.
 *
 * A step never waits for a period: the states sample and print on their own
 * periods measured with self->get_tick, and the LEDs blink on their own once
 * set. A step still lasts as long as its callbacks: the POST step runs
 * self->self_test, and a waiting or paused step may run a whole CLI command.
 *
 * \param self The SystemFsm instance.
 * \return SYSTEM_FSM_OK, SYSTEM_FSM_SELF_IS_NULL or SYSTEM_FSM_INVALID_TRANSITION.
 */
enum SystemFsmStatus system_fsm_run(struct SystemFsm *self);

/*!
 * \brief Get the current state.
 *
 * A requested transition shows only after the next system_fsm_run() applies it.
 *
 * \param self The SystemFsm instance; must not be NULL.
 * \return The current state.
 */
enum SystemFsmState system_fsm_get_state(const struct SystemFsm *self);

/*!
 * \brief Check whether the CLI may accept input.
 *
 * Safe to call from the UART ISR to decide whether to feed a received character.
 *
 * \param self The SystemFsm instance; must not be NULL, otherwise unused.
 * \return true in WAITING and PAUSED, false otherwise.
 */
bool system_fsm_is_cli_enabled(const struct SystemFsm *self);

/*!
 * \brief Post an external event, requesting the transition it causes in the current state.
 *
 * The transition is only requested here: the state switch and its enter
 * function run at the end of the next system_fsm_run(). The one exception is
 * a click in FATAL, which calls self->reset_mcu right away.
 *
 * Does nothing if self is NULL, if trigger is not valid or if the current
 * state ignores the trigger.
 *
 * \attention When called from an interrupt (e.g. the button EXTI), the request
 * shares the pending slot with the state routines: if both request a
 * transition in the same step, the last one written wins and the other is lost.
 * A click in FATAL resets the MCU from inside the interrupt.
 *
 * \param self    The SystemFsm instance.
 * \param trigger The event to post.
 */
void system_fsm_post_trigger(struct SystemFsm *self, enum SystemFsmTrigger trigger);

#endif
