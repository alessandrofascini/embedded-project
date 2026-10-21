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
#include "float-format-api.h"

#include "system-fsm-cli.h"
#include "system-fsm-fatal.h"
#include "system-fsm-leds.h"
#include "system-fsm-time.h"

#define SYSTEM_FSM_LISTENING_PERIOD_MS (100U)
/* NTC - POT at which the external LED reaches full duty */
#define SYSTEM_FSM_LISTENING_THRESHOLD_CELSIUS (5.0f)
#define SYSTEM_FSM_DUTY_MAX_PERCENT (100U)
/* how long NTC - POT must stay above the threshold before the warning */
#define SYSTEM_FSM_LISTENING_WARNING_MS (5000U)
/* same rate as listening: the stream keeps its cadence and the filters their time constant */
#define SYSTEM_FSM_WARNING_SAMPLE_PERIOD_MS (100U)
#define SYSTEM_FSM_WARNING_MESSAGE_PERIOD_MS (200U)
/* how long the warning may last before it becomes fatal */
#define SYSTEM_FSM_WARNING_TIMEOUT_MS (10000U)
#define SYSTEM_FSM_CELSIUS_STRING_LENGTH (10U)

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

typedef void (*system_fsm_trigger_handler)(struct SystemFsm *self);

static void prv_trigger_go_listening(struct SystemFsm *self);
static void prv_trigger_go_paused(struct SystemFsm *self);
static void prv_trigger_go_waiting(struct SystemFsm *self);
static void prv_transition_to(struct SystemFsm *self, enum SystemFsmState state);

static inline uint8_t prv_difference_to_duty_percent(float difference_celsius, float threshold_celsius);
static inline void prv_listening_check_warning(struct SystemFsm *self, float difference_celsius);
static const char *prv_format_celsius(float celsius, char *buffer, uint16_t buffer_size);
static void prv_print_reading(const struct SystemFsm *self, const struct SystemFsmReading *reading);

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

/* what each trigger does in each state: a missing entry is NULL, the trigger is ignored there */
static const system_fsm_trigger_handler prv_trigger_handlers[SYSTEM_FSM_STATE_COUNT][SYSTEM_FSM_TRIGGER_COUNT] = {
    [SYSTEM_FSM_STATE_WAITING] = {
        [SYSTEM_FSM_TRIGGER_BUTTON_CLICK] = prv_trigger_go_listening,
        [SYSTEM_FSM_TRIGGER_RUN_COMMAND] = prv_trigger_go_listening,
    },
    [SYSTEM_FSM_STATE_LISTENING] = {
        [SYSTEM_FSM_TRIGGER_BUTTON_CLICK] = prv_trigger_go_paused,
    },
    [SYSTEM_FSM_STATE_PAUSED] = {
        [SYSTEM_FSM_TRIGGER_BUTTON_CLICK] = prv_trigger_go_listening,
        [SYSTEM_FSM_TRIGGER_RUN_COMMAND] = prv_trigger_go_listening,
    },
    [SYSTEM_FSM_STATE_WARNING] = {
        /* a plain click is ignored here: only a long press leaves the warning */
        [SYSTEM_FSM_TRIGGER_BUTTON_HOLD] = prv_trigger_go_waiting,
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
    assert(self->leds->set_duty != NULL);

    if (self->read_sensors == NULL) {
        return SYSTEM_FSM_READ_SENSORS_IS_NULL;
    }
    if (self->print == NULL) {
        return SYSTEM_FSM_PRINT_IS_NULL;
    }

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

/* TODO: called from the button EXTI ISR, so the transition (and the enter function) runs in
 * interrupt context. If it preempts a routine that is mid-sample, the routine resumes in the new
 * state and may request a transition that is not allowed from there (e.g. paused -> warning),
 * which trips the assert in prv_transition_to. Store the trigger in a volatile pending field here
 * and consume it at the start of system_fsm_run, so every transition happens in the main loop. */
void system_fsm_post_trigger(struct SystemFsm *self, enum SystemFsmTrigger trigger) {
    if (self == NULL) {
        return;
    }
    const enum SystemFsmState state = system_fsm_get_state(self);
    if (state >= SYSTEM_FSM_STATE_COUNT || trigger >= SYSTEM_FSM_TRIGGER_COUNT) {
        return;
    }
    const system_fsm_trigger_handler handler = prv_trigger_handlers[state][trigger];
    if (handler != NULL) {
        handler(self);
    }
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
    struct SystemFsm *self = data;
    assert(self != NULL);
    if (!int_system_fsm_has_passed_ms_and_update(self->get_tick(), &self->listening.last_sample_ms, SYSTEM_FSM_LISTENING_PERIOD_MS)) {
        return;
    }
    struct SystemFsmReading reading;
    self->read_sensors(&reading);
    prv_print_reading(self, &reading);

    const float difference_celsius = reading.ntc_celsius - reading.pot_celsius;
    const uint8_t duty_percent = prv_difference_to_duty_percent(difference_celsius, SYSTEM_FSM_LISTENING_THRESHOLD_CELSIUS);
    self->leds->set_duty(SYSTEM_FSM_LED_EXTERNAL, duty_percent);

    prv_listening_check_warning(self, difference_celsius);
}

static void prv_routine_warning(void *data) {
    struct SystemFsm *self = data;
    assert(self != NULL);
    if (int_system_fsm_has_passed_ms(self->get_tick(), self->warning.entered_ms, SYSTEM_FSM_WARNING_TIMEOUT_MS)) {
        int_system_fsm_go_fatal(self, SYSTEM_FSM_FATAL_ERROR_WARNING_TIMEOUT);
        return;
    }
    /* sensor reading stays on: keep streaming the readings as in listening */
    if (int_system_fsm_has_passed_ms_and_update(self->get_tick(), &self->warning.last_sample_ms, SYSTEM_FSM_WARNING_SAMPLE_PERIOD_MS)) {
        struct SystemFsmReading reading;
        self->read_sensors(&reading);
        prv_print_reading(self, &reading);
    }
    if (int_system_fsm_has_passed_ms_and_update(self->get_tick(), &self->warning.last_message_ms, SYSTEM_FSM_WARNING_MESSAGE_PERIOD_MS)) {
        self->print("WARNING\r\n");
    }
}

static void prv_routine_paused(void *data) {
    struct SystemFsm *self = data;
    assert(self != NULL && self->cli != NULL);
    self->cli->print();
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
    struct SystemFsm *self = data;
    assert(self != NULL);
    /* restart the sampling period: a stale timestamp from a previous visit would fire at once */
    self->listening.last_sample_ms = self->get_tick();
    /* the warning window must not carry over from a previous visit (e.g. through pause) */
    self->listening.is_above = false;
    int_system_fsm_cli_disable();
    int_system_fsm_leds_apply(self->leds, SYSTEM_FSM_STATE_LISTENING);
}

static void prv_enter_warning(void *data) {
    struct SystemFsm *self = data;
    assert(self != NULL);
    const uint32_t now_ms = self->get_tick();
    self->warning.entered_ms = now_ms;
    self->warning.last_sample_ms = now_ms;
    /* one period in the past: the first WARNING goes out as soon as the state is entered */
    self->warning.last_message_ms = now_ms - SYSTEM_FSM_WARNING_MESSAGE_PERIOD_MS;
    int_system_fsm_cli_disable();
    int_system_fsm_leds_apply(self->leds, SYSTEM_FSM_STATE_WARNING);
}

static void prv_enter_paused(void *data) {
    const struct SystemFsm *self = data;
    assert(self != NULL);
    int_system_fsm_cli_enable(self->cli);
    int_system_fsm_leds_apply(self->leds, SYSTEM_FSM_STATE_PAUSED);
}

static void prv_enter_fatal(void *data) {
    int_system_fsm_cli_disable();
}

static void prv_trigger_go_listening(struct SystemFsm *self) {
    prv_transition_to(self, SYSTEM_FSM_STATE_LISTENING);
}

static void prv_trigger_go_paused(struct SystemFsm *self) {
    prv_transition_to(self, SYSTEM_FSM_STATE_PAUSED);
}

static void prv_trigger_go_waiting(struct SystemFsm *self) {
    prv_transition_to(self, SYSTEM_FSM_STATE_WAITING);
}

static void prv_transition_to(struct SystemFsm *self, enum SystemFsmState state) {
    assert(self != NULL);
    const enum FSMReturnCode result = fsm_api_transition_to(&self->handler, state);
    assert(result == FSM_RC_OK);
}

static inline uint8_t prv_difference_to_duty_percent(float difference_celsius, float threshold_celsius) {
    assert(threshold_celsius > 0.0f);
    /* linear: 0 % at no difference, 100 % at the threshold, clamped outside */
    /* TODO: a NaN difference fails both checks and reaches the uint8_t cast, which is undefined
     * behaviour. Write the first check as !(difference_celsius > 0.0f) so NaN maps to 0 %. */
    if (difference_celsius <= 0.0f) {
        return 0U;
    }
    if (difference_celsius >= threshold_celsius) {
        return SYSTEM_FSM_DUTY_MAX_PERCENT;
    }
    return (uint8_t)(difference_celsius * SYSTEM_FSM_DUTY_MAX_PERCENT / threshold_celsius);
}

static inline void prv_listening_check_warning(struct SystemFsm *self, float difference_celsius) {
    assert(self != NULL);
    struct SystemFsmListening *listening = &self->listening;
    /* the 5 s must be continuous: one sample at or below the threshold restarts the window */
    /* TODO: a NaN difference fails this check and counts as above the threshold; decide whether
     * it should, and if not write it as !(difference_celsius > SYSTEM_FSM_LISTENING_THRESHOLD_CELSIUS). */
    if (difference_celsius <= SYSTEM_FSM_LISTENING_THRESHOLD_CELSIUS) {
        listening->is_above = false;
        return;
    }
    const uint32_t now_ms = self->get_tick();
    if (!listening->is_above) {
        listening->is_above = true;
        listening->above_since_ms = now_ms;
        return;
    }
    if (int_system_fsm_has_passed_ms(now_ms, listening->above_since_ms, SYSTEM_FSM_LISTENING_WARNING_MS)) {
        prv_transition_to(self, SYSTEM_FSM_STATE_WARNING);
    }
}

static const char *prv_format_celsius(float celsius, char *buffer, uint16_t buffer_size) {
    const enum FloatFormatError result = float_to_string(celsius, buffer, buffer_size);
    /* on failure the buffer content is undefined: never print it */
    if (result == FLOAT_FORMAT_ERR_NAN) {
        return "nan";
    }
    if (result != FLOAT_FORMAT_OK) {
        return "err";
    }
    return buffer;
}

static void prv_print_reading(const struct SystemFsm *self, const struct SystemFsmReading *reading) {
    assert(self != NULL && reading != NULL);
    char pot_buffer[SYSTEM_FSM_CELSIUS_STRING_LENGTH];
    char ntc_buffer[SYSTEM_FSM_CELSIUS_STRING_LENGTH];
    const char *pot_celsius = prv_format_celsius(reading->pot_celsius, pot_buffer, sizeof(pot_buffer));
    const char *ntc_celsius = prv_format_celsius(reading->ntc_celsius, ntc_buffer, sizeof(ntc_buffer));
    self->print("POT_RAW:%u,POT_C:%s,NTC_RAW:%u,NTC_C:%s\r\n", reading->pot_raw, pot_celsius, reading->ntc_raw, ntc_celsius);
}
