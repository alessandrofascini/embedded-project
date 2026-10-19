/*!
 * \file system-fsm.h
 * \author Alessandro Fascini
 * \brief Types of the system FSM.
 */

#ifndef RECRUITING_SYSTEM_FSM_H
#define RECRUITING_SYSTEM_FSM_H

#include <stdint.h>

#include "eagletrt-fsm.h"

enum SystemFsmFatalErrorCode {
    SYSTEM_FSM_FATAL_ERROR_NONE,
    SYSTEM_FSM_FATAL_ERROR_UNKNOWN,
    SYSTEM_FSM_FATAL_ERROR_SERIAL_START_FAILED,
    SYSTEM_FSM_FATAL_ERROR_SENSOR_START_FAILED,
    SYSTEM_FSM_FATAL_ERROR_SENSOR_TIMEOUT,
    SYSTEM_FSM_FATAL_ERROR_NTC_OUT_OF_RANGE,
    SYSTEM_FSM_FATAL_ERROR_COUNT,
};

typedef enum SystemFsmFatalErrorCode (*system_fsm_self_test)(void);
typedef uint32_t (*system_fsm_get_tick)(void);

enum SystemFsmStatus {
    SYSTEM_FSM_OK,
    SYSTEM_FSM_SELF_IS_NULL,
    SYSTEM_FSM_INVALID_MACHINE,
    SYSTEM_FSM_INVALID_TRANSITION,
    SYSTEM_FSM_PORT_IS_NULL,
    SYSTEM_FSM_GET_TICK_IS_NULL,
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

struct SystemFsmPort {
    system_fsm_self_test self_test;
};

struct SystemFsmFatal {
    enum SystemFsmFatalErrorCode error;
    uint32_t last_message_ms;
};

struct SystemFsm {
    struct FSMHandler handler;
    const struct SystemFsmPort *port;
    struct SystemFsmFatal fatal;
    system_fsm_get_tick get_tick;
};

#endif
