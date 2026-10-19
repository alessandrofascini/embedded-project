/*!
 * \file system-fsm-api.c
 * \author Alessandro Fascini
 * \brief Implementation of the system FSM.
 */

#include "system-fsm-api.h"

#include "eagletrt-fsm-api.h"

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#define SYSTEM_FSM_FATAL_PERIOD_MS (100U)

static void prv_go_fatal(struct SystemFsm *self, enum SystemFsmFatalErrorCode code);
static const char *prv_fatal_message(enum SystemFsmFatalErrorCode code);
static inline bool prv_has_passed_millis(uint32_t tick, uint32_t last_ms, uint32_t duration_ms);
static bool prv_has_passed_millis_and_update(struct SystemFsm *self, uint32_t *last_ms, uint32_t duration_ms);

static void prv_routine_post(void *data);
static void prv_routine_waiting(void *data);
static void prv_routine_listening(void *data);
static void prv_routine_warning(void *data);
static void prv_routine_paused(void *data);
static void prv_routine_fatal(void *data);

static void prv_enter_waiting(void *data);
static void prv_enter_listening(void *data);
static void prv_enter_warning(void *data);
static void prv_enter_paused(void *data);
static void prv_enter_fatal(void *data);

static const char *const prv_fatal_messages[SYSTEM_FSM_FATAL_ERROR_COUNT] = {
    [SYSTEM_FSM_FATAL_ERROR_NONE] = "no error",
    [SYSTEM_FSM_FATAL_ERROR_UNKNOWN] = "unknown fatal error",
    [SYSTEM_FSM_FATAL_ERROR_SERIAL_START_FAILED] = "failed to start the serial reception",
    [SYSTEM_FSM_FATAL_ERROR_SENSOR_START_FAILED] = "failed to start the sensors",
    [SYSTEM_FSM_FATAL_ERROR_SENSOR_TIMEOUT] = "no sensor conversion within the timeout",
    [SYSTEM_FSM_FATAL_ERROR_NTC_OUT_OF_RANGE] = "NTC reading at a rail: open or shorted",
};

static const struct Transition prv_post_transitions[] = {
    { .to = SYSTEM_FSM_STATE_WAITING, .function = prv_enter_waiting },
    { .to = SYSTEM_FSM_STATE_FATAL, .function = prv_enter_fatal },
};

static const struct Transition prv_waiting_transitions[] = {
    { .to = SYSTEM_FSM_STATE_LISTENING, .function = prv_enter_listening },
    { .to = SYSTEM_FSM_STATE_FATAL, .function = prv_enter_fatal },
};

static const struct Transition prv_listening_transitions[] = {
    { .to = SYSTEM_FSM_STATE_WARNING, .function = prv_enter_warning },
    { .to = SYSTEM_FSM_STATE_PAUSED, .function = prv_enter_paused },
    { .to = SYSTEM_FSM_STATE_FATAL, .function = prv_enter_fatal },
};

static const struct Transition prv_warning_transitions[] = {
    { .to = SYSTEM_FSM_STATE_WAITING, .function = prv_enter_waiting },
    { .to = SYSTEM_FSM_STATE_FATAL, .function = prv_enter_fatal },
};

static const struct Transition prv_paused_transitions[] = {
    { .to = SYSTEM_FSM_STATE_LISTENING, .function = prv_enter_listening },
    { .to = SYSTEM_FSM_STATE_FATAL, .function = prv_enter_fatal },
};

static const struct State prv_states[SYSTEM_FSM_STATE_COUNT] = {
    [SYSTEM_FSM_STATE_POST] = {
        .id = SYSTEM_FSM_STATE_POST,
        .function = prv_routine_post,
        .next_default = SYSTEM_FSM_STATE_FATAL,
        .transitions = prv_post_transitions,
        .num_transitions = EAGLETRT_FSM_TRANSITION_LEN(prv_post_transitions),
    },
    [SYSTEM_FSM_STATE_WAITING] = {
        .id = SYSTEM_FSM_STATE_WAITING,
        .function = prv_routine_waiting,
        .next_default = SYSTEM_FSM_STATE_WAITING,
        .transitions = prv_waiting_transitions,
        .num_transitions = EAGLETRT_FSM_TRANSITION_LEN(prv_waiting_transitions),
    },
    [SYSTEM_FSM_STATE_LISTENING] = {
        .id = SYSTEM_FSM_STATE_LISTENING,
        .function = prv_routine_listening,
        .next_default = SYSTEM_FSM_STATE_LISTENING,
        .transitions = prv_listening_transitions,
        .num_transitions = EAGLETRT_FSM_TRANSITION_LEN(prv_listening_transitions),
    },
    [SYSTEM_FSM_STATE_WARNING] = {
        .id = SYSTEM_FSM_STATE_WARNING,
        .function = prv_routine_warning,
        .next_default = SYSTEM_FSM_STATE_WARNING,
        .transitions = prv_warning_transitions,
        .num_transitions = EAGLETRT_FSM_TRANSITION_LEN(prv_warning_transitions),
    },
    [SYSTEM_FSM_STATE_PAUSED] = {
        .id = SYSTEM_FSM_STATE_PAUSED,
        .function = prv_routine_paused,
        .next_default = SYSTEM_FSM_STATE_PAUSED,
        .transitions = prv_paused_transitions,
        .num_transitions = EAGLETRT_FSM_TRANSITION_LEN(prv_paused_transitions),
    },
    [SYSTEM_FSM_STATE_FATAL] = {
        .id = SYSTEM_FSM_STATE_FATAL,
        .function = prv_routine_fatal,
        .next_default = SYSTEM_FSM_STATE_FATAL,
        .transitions = NULL,
        .num_transitions = 0,
    },
};

enum SystemFsmStatus system_fsm_init(struct SystemFsm *self) {
    if (self == NULL) {
        return SYSTEM_FSM_SELF_IS_NULL;
    }
    if (self->port == NULL) {
        return SYSTEM_FSM_PORT_IS_NULL;
    }
    if (self->get_tick == NULL) {
        return SYSTEM_FSM_GET_TICK_IS_NULL;
    }
    self->fatal.error = SYSTEM_FSM_FATAL_ERROR_NONE;
    self->fatal.last_message_ms = 0;

    const enum FSMReturnCode result = fsm_api_init(&self->handler, prv_states, SYSTEM_FSM_STATE_COUNT, SYSTEM_FSM_STATE_POST);
    if (result != FSM_RC_OK) {
        return SYSTEM_FSM_INVALID_MACHINE;
    }

    return SYSTEM_FSM_OK;
}

enum SystemFsmStatus system_fsm_run(struct SystemFsm *self) {
    if (self == NULL) {
        return SYSTEM_FSM_SELF_IS_NULL;
    }
    const enum FSMReturnCode result = fsm_api_run_state(&self->handler, self);
    if (result != FSM_RC_OK) {
        return SYSTEM_FSM_INVALID_TRANSITION;
    }
    return SYSTEM_FSM_OK;
}

static void prv_go_fatal(struct SystemFsm *self, enum SystemFsmFatalErrorCode code) {
    assert(self != NULL);
    if (self->fatal.error == SYSTEM_FSM_FATAL_ERROR_NONE) {
        self->fatal.error = code;
    }
    fsm_api_transition_to(&self->handler, SYSTEM_FSM_STATE_FATAL);
}

static const char *prv_fatal_message(enum SystemFsmFatalErrorCode code) {
    if (code >= SYSTEM_FSM_FATAL_ERROR_COUNT || prv_fatal_messages[code] == NULL) {
        return prv_fatal_messages[SYSTEM_FSM_FATAL_ERROR_UNKNOWN];
    }
    return prv_fatal_messages[code];
}

static inline bool prv_has_passed_millis(uint32_t tick, uint32_t last_ms, uint32_t duration_ms) {
    const uint32_t elapsed_ms = tick - last_ms;
    return elapsed_ms >= duration_ms;
}

static bool prv_has_passed_millis_and_update(struct SystemFsm *self, uint32_t *last_ms, uint32_t duration_ms) {
    assert(self != NULL);
    assert(last_ms != NULL);
    const uint32_t current_tick = self->get_tick();
    if (prv_has_passed_millis(current_tick, *last_ms, duration_ms)) {
        *last_ms = current_tick;
        return true;
    }
    return false;
}

static void prv_routine_post(void *data) {
    struct SystemFsm *self = data;
    const struct SystemFsmPort *port = self->port;

    const enum SystemFsmFatalErrorCode error = port->self_test();
    if (error != SYSTEM_FSM_FATAL_ERROR_NONE) {
        prv_go_fatal(self, error);
        return;
    }
    /* Everything is fine, lets move to the waiting state */
    fsm_api_transition_to(&self->handler, SYSTEM_FSM_STATE_WAITING);
}

static void prv_routine_waiting(void *data) {
    printf("[FSM] routine: WAITING\r\n");
}

static void prv_routine_listening(void *data) {
    printf("[FSM] routine: LISTENING\r\n");
}

static void prv_routine_warning(void *data) {
    printf("[FSM] routine: WARNING\r\n");
}

static void prv_routine_paused(void *data) {
    printf("[FSM] routine: PAUSED\r\n");
}

static void prv_routine_fatal(void *data) {
    struct SystemFsm *self = data;
    assert(self != NULL);
    if (prv_has_passed_millis_and_update(self, &self->fatal.last_message_ms, SYSTEM_FSM_FATAL_PERIOD_MS)) {
        printf("[ERROR] %s (code: %d)\r\n", prv_fatal_message(self->fatal.error), self->fatal.error);
    }
}

static void prv_enter_waiting(void *data) {
    printf("[FSM] enter: WAITING\r\n");
}

static void prv_enter_listening(void *data) {
    printf("[FSM] enter: LISTENING\r\n");
}

static void prv_enter_warning(void *data) {
    printf("[FSM] enter: WARNING\r\n");
}

static void prv_enter_paused(void *data) {
    printf("[FSM] enter: PAUSED\r\n");
}

static void prv_enter_fatal(void *data) {
    /* do nothing */
}
