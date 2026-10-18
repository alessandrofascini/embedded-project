/*!
 * \file system-fsm-api.c
 * \author Alessandro Fascini
 * \brief Implementation of the system FSM.
 */

#include "system-fsm-api.h"

#include "eagletrt-fsm-api.h"

#include <stddef.h>
#include <stdio.h>

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

static void prv_routine_post(void *data) {
    printf("[FSM] routine: POST\r\n");
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
    printf("[FSM] routine: FATAL\r\n");
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
    printf("[FSM] enter: FATAL\r\n");
}
