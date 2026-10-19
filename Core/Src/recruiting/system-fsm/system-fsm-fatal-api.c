/*!
 * \file system-fsm-fatal-api.c
 * \author Alessandro Fascini
 * \brief Fatal error handling of the system FSM: error recording and periodic report.
 */

#include <stddef.h>
#include <stdio.h>
#include <assert.h>

#include "eagletrt-fsm-api.h"

#include "system-fsm-fatal.h"
#include "system-fsm-time.h"

#define SYSTEM_FSM_FATAL_PERIOD_MS (100U)

static const char *prv_fatal_message(enum SystemFsmFatalErrorCode code);

static const char *const prv_fatal_messages[SYSTEM_FSM_FATAL_ERROR_COUNT] = {
    [SYSTEM_FSM_FATAL_ERROR_NONE] = "no error",
    [SYSTEM_FSM_FATAL_ERROR_UNKNOWN] = "unknown fatal error",
    [SYSTEM_FSM_FATAL_ERROR_SERIAL_START_FAILED] = "failed to start the serial reception",
    [SYSTEM_FSM_FATAL_ERROR_SENSOR_START_FAILED] = "failed to start the sensors",
    [SYSTEM_FSM_FATAL_ERROR_SENSOR_TIMEOUT] = "no sensor conversion within the timeout",
    [SYSTEM_FSM_FATAL_ERROR_NTC_OUT_OF_RANGE] = "NTC reading at a rail: open or shorted",
    [SYSTEM_FSM_FATAL_ERROR_LED_START_FAILED] = "failed to start the LED timers",
};

void int_system_fsm_fatal_init(struct SystemFsmFatal *fatal) {
    assert(fatal != NULL);
    fatal->error = SYSTEM_FSM_FATAL_ERROR_NONE;
    fatal->last_message_ms = 0;
}

void int_system_fsm_go_fatal(struct SystemFsm *self, enum SystemFsmFatalErrorCode code) {
    assert(self != NULL);
    /* keep the first error: it is the root cause */
    if (self->fatal.error == SYSTEM_FSM_FATAL_ERROR_NONE) {
        self->fatal.error = code;
    }
    fsm_api_transition_to(&self->handler, SYSTEM_FSM_STATE_FATAL);
}

void int_system_fsm_fatal_report(struct SystemFsm *self) {
    assert(self != NULL);
    if (int_system_fsm_has_passed_ms_and_update(self->get_tick(), &self->fatal.last_message_ms, SYSTEM_FSM_FATAL_PERIOD_MS)) {
        printf("[ERROR] %s (code: %d)\r\n", prv_fatal_message(self->fatal.error), self->fatal.error);
    }
}

static const char *prv_fatal_message(enum SystemFsmFatalErrorCode code) {
    if (code >= SYSTEM_FSM_FATAL_ERROR_COUNT || prv_fatal_messages[code] == NULL) {
        return prv_fatal_messages[SYSTEM_FSM_FATAL_ERROR_UNKNOWN];
    }
    return prv_fatal_messages[code];
}
