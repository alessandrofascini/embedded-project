/*!
 * \file system-fsm-api.c
 * \author Alessandro Fascini
 * \brief Implementation of the system FSM.
 */

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include "system-fsm-api.h"

#include "eagletrt-fsm-api.h"

#include "system-fsm-cli.h"
#include "system-fsm-fatal.h"
#include "system-fsm-leds.h"

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
    if (self->self_test == NULL) {
        return SYSTEM_FSM_SELF_TEST_IS_NULL;
    }

    if (self->cli == NULL) {
        return SYSTEM_FSM_CLI_IS_NULL;
    }
    assert(self->cli->print != NULL);
    assert(self->cli->reset != NULL);

    if (self->leds == NULL) {
        return SYSTEM_FSM_LEDS_IS_NULL;
    }
    assert(self->leds->set_blink != NULL);
    assert(self->leds->set_breathing != NULL);

    if (self->get_tick == NULL) {
        return SYSTEM_FSM_GET_TICK_IS_NULL;
    }

    const enum FSMReturnCode result = fsm_api_init(&self->handler, prv_states, SYSTEM_FSM_STATE_COUNT, SYSTEM_FSM_STATE_POST);
    if (result != FSM_RC_OK) {
        return SYSTEM_FSM_INVALID_MACHINE;
    }

    int_system_fsm_fatal_init(&self->fatal);
    int_system_fsm_cli_disable();

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

enum SystemFsmState system_fsm_get_state(const struct SystemFsm *self) {
    assert(self != NULL);
    return fsm_api_get_state(&self->handler);
}

static void prv_routine_post(void *data) {
    struct SystemFsm *self = data;
    assert(self != NULL);
    const enum SystemFsmFatalErrorCode error = self->self_test();
    if (error != SYSTEM_FSM_FATAL_ERROR_NONE) {
        int_system_fsm_go_fatal(self, error);
        return;
    }
    /* Everything is fine, lets move to the waiting state */
    fsm_api_transition_to(&self->handler, SYSTEM_FSM_STATE_WAITING);
}

static void prv_routine_waiting(void *data) {
    struct SystemFsm *self = data;
    assert(self != NULL && self->cli != NULL);
    self->cli->print();
}

static void prv_routine_listening(void *data) {
    /* do nothing */
}

static void prv_routine_warning(void *data) {
    /* do nothing */
}

static void prv_routine_paused(void *data) {
    /* do nothing */
}

static void prv_routine_fatal(void *data) {
    struct SystemFsm *self = data;
    assert(self != NULL);
    int_system_fsm_fatal_report(self);
}

static void prv_enter_waiting(void *data) {
    const struct SystemFsm *self = data;
    assert(self != NULL);
    int_system_fsm_cli_enable(self->cli);
    int_system_fsm_leds_apply(self->leds, SYSTEM_FSM_STATE_WAITING);
}

static void prv_enter_listening(void *data) {
    int_system_fsm_cli_disable();
}

static void prv_enter_warning(void *data) {
    int_system_fsm_cli_disable();
}

static void prv_enter_paused(void *data) {
    const struct SystemFsm *self = data;
    assert(self != NULL);
    int_system_fsm_cli_enable(self->cli);
}

static void prv_enter_fatal(void *data) {
    int_system_fsm_cli_disable();
}
