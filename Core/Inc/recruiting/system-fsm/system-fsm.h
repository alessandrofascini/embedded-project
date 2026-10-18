/*!
 * \file system-fsm.h
 * \author Alessandro Fascini
 * \brief Types of the system FSM.
 */

#ifndef RECRUITING_SYSTEM_FSM_H
#define RECRUITING_SYSTEM_FSM_H

#include "eagletrt-fsm.h"

enum SystemFsmStatus {
    SYSTEM_FSM_OK = 0,
    SYSTEM_FSM_SELF_IS_NULL = -1,
    SYSTEM_FSM_INVALID_MACHINE = -2,
    SYSTEM_FSM_INVALID_TRANSITION = -3,
};

enum SystemFsmState {
    SYSTEM_FSM_STATE_POST,
    SYSTEM_FSM_STATE_WAITING,
    SYSTEM_FSM_STATE_LISTENING,
    SYSTEM_FSM_STATE_WARNING,
    SYSTEM_FSM_STATE_PAUSED,
    SYSTEM_FSM_STATE_FATAL,
    SYSTEM_FSM_STATE_COUNT,
};

struct SystemFsm {
    struct FSMHandler handler;
};

#endif
