/*!
 * \file system-fsm-leds.h
 * \author Alessandro Fascini
 * \brief Private LED helpers of the system FSM, shared only inside the module.
 */

#ifndef RECRUITING_SYSTEM_FSM_LEDS_H
#define RECRUITING_SYSTEM_FSM_LEDS_H

#include "system-fsm.h"

void int_system_fsm_leds_apply(const struct SystemFsmLeds *leds, enum SystemFsmState state);

#endif
