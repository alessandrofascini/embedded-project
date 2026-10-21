/*!
 * \file system-fsm-leds.h
 * \author Alessandro Fascini
 * \brief Private LED helpers of the system FSM, shared only inside the module.
 */

#ifndef RECRUITING_SYSTEM_FSM_LEDS_H
#define RECRUITING_SYSTEM_FSM_LEDS_H

#include "system-fsm.h"

/*!
 * \brief Set every LED to the pattern of a state.
 *
 * In LISTENING the external LED is switched off here; the listening routine
 * then drives its duty from NTC - POT.
 *
 * \param leds  The LED callbacks; must not be NULL.
 * \param state The state being entered; any state except POST, which has no pattern.
 */
void int_system_fsm_leds_apply(const struct SystemFsmLeds *leds, enum SystemFsmState state);

#endif
