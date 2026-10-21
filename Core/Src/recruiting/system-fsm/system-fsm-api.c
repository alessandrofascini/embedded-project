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
#include "adc-filter-api.h"
#include "ntc-api.h"

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

/*!
 * \brief One filtered sample of the potentiometer and the NTC.
 */
struct SystemFsmReading {
    uint16_t pot_raw;  /*!< Potentiometer ADC value after the active filters. */
    float pot_celsius; /*!< Potentiometer value mapped to a temperature in °C. */
    uint16_t ntc_raw;  /*!< NTC ADC value after the active filters. */
    float ntc_celsius; /*!< NTC temperature in °C. */
};

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
static void prv_trigger_reset_mcu(struct SystemFsm *self);
static void prv_transition_to(struct SystemFsm *self, enum SystemFsmState state);

static inline uint8_t prv_difference_to_duty_percent(float difference_celsius, float threshold_celsius);
static inline void prv_listening_check_warning(struct SystemFsm *self, float difference_celsius);
static const char *prv_format_celsius(float celsius, char *buffer, uint16_t buffer_size);
static inline void prv_read_sensors(struct SystemFsm *self, struct SystemFsmReading *reading);
static void prv_print_reading(const struct SystemFsm *self, const struct SystemFsmReading *reading);
static inline enum SystemFsmStatus prv_nested_status(enum SystemFsmStatus base, int code);

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
    [SYSTEM_FSM_STATE_FATAL] = {
        [SYSTEM_FSM_TRIGGER_BUTTON_CLICK] = prv_trigger_reset_mcu,
    },
};

enum SystemFsmStatus system_fsm_init(struct SystemFsm *self, const struct SystemFsmConfig *config) {
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

    if (self->read_raw == NULL) {
        return SYSTEM_FSM_READ_RAW_IS_NULL;
    }
    if (self->print == NULL) {
        return SYSTEM_FSM_PRINT_IS_NULL;
    }
    if (self->reset_mcu == NULL) {
        return SYSTEM_FSM_RESET_MCU_IS_NULL;
    }

    if (self->get_tick == NULL) {
        return SYSTEM_FSM_GET_TICK_IS_NULL;
    }

    if (config == NULL) {
        return SYSTEM_FSM_CONFIG_IS_NULL;
    }
    const enum AdcFilterError pot_filter_result = adc_filter_init(&self->pot_filter);
    if (pot_filter_result != ADC_FILTER_OK) {
        return prv_nested_status(SYSTEM_FSM_POT_FILTER_INIT_FAILED, pot_filter_result);
    }
    const enum AdcFilterError ntc_filter_result = adc_filter_init(&self->ntc_filter);
    if (ntc_filter_result != ADC_FILTER_OK) {
        return prv_nested_status(SYSTEM_FSM_NTC_FILTER_INIT_FAILED, ntc_filter_result);
    }
    const enum NtcError pot_model_result = ntc_init(&self->pot, &config->pot);
    if (pot_model_result != NTC_OK) {
        return prv_nested_status(SYSTEM_FSM_POT_MODEL_INIT_FAILED, pot_model_result);
    }
    const enum NtcError ntc_model_result = ntc_init(&self->ntc, &config->ntc);
    if (ntc_model_result != NTC_OK) {
        return prv_nested_status(SYSTEM_FSM_NTC_MODEL_INIT_FAILED, ntc_model_result);
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

/* TODO: called from the button EXTI ISR. The handlers only request a transition (the switch and
 * the enter function run later, in system_fsm_run), but the request shares requested_state with
 * the state routines: if the ISR and a routine both request in the same step (e.g. a click in
 * listening requests paused while the routine requests warning), the last write wins and the other is lost. A
 * click in fatal also calls reset_mcu from inside the ISR. Store the trigger in a volatile pending
 * field here and consume it at the start of system_fsm_run, so every trigger is handled in the main loop. */
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
    prv_read_sensors(self, &reading);
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
        prv_read_sensors(self, &reading);
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
    const struct SystemFsm *self = data;
    assert(self != NULL);
    int_system_fsm_cli_disable();
    int_system_fsm_leds_apply(self->leds, SYSTEM_FSM_STATE_FATAL);
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

static void prv_trigger_reset_mcu(struct SystemFsm *self) {
    assert(self != NULL);
    /* fatal has no outgoing transition: a reset is the only way out, and it does not return */
    self->reset_mcu();
}

static void prv_transition_to(struct SystemFsm *self, enum SystemFsmState state) {
    assert(self != NULL);
    const enum FSMReturnCode result = fsm_api_transition_to(&self->handler, state);
    assert(result == FSM_RC_OK);
}

static inline uint8_t prv_difference_to_duty_percent(float difference_celsius, float threshold_celsius) {
    assert(threshold_celsius > 0.0F);
    /* linear: 0 % at no difference, 100 % at the threshold, clamped outside */
    /* TODO: a NaN difference fails both checks and reaches the uint8_t cast, which is undefined
     * behaviour. Write the first check as !(difference_celsius > 0.0F) so NaN maps to 0 %. */
    if (difference_celsius <= 0.0F) {
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

static inline void prv_read_sensors(struct SystemFsm *self, struct SystemFsmReading *reading) {
    assert(self != NULL && reading != NULL);
    uint16_t pot_raw;
    uint16_t ntc_raw;
    self->read_raw(&pot_raw, &ntc_raw);
    /* the latest conversion through the active filters, then into degrees */
    reading->pot_raw = adc_filter_apply(&self->pot_filter, pot_raw);
    reading->pot_celsius = ntc_raw_adc_to_celsius(&self->pot, reading->pot_raw);
    reading->ntc_raw = adc_filter_apply(&self->ntc_filter, ntc_raw);
    reading->ntc_celsius = ntc_raw_adc_to_celsius(&self->ntc, reading->ntc_raw);
}

static void prv_print_reading(const struct SystemFsm *self, const struct SystemFsmReading *reading) {
    assert(self != NULL && reading != NULL);

    char pot_buffer[SYSTEM_FSM_CELSIUS_STRING_LENGTH];
    const char *pot_celsius_str = prv_format_celsius(reading->pot_celsius, pot_buffer, sizeof(pot_buffer));

    char ntc_buffer[SYSTEM_FSM_CELSIUS_STRING_LENGTH];
    const char *ntc_celsius_str = prv_format_celsius(reading->ntc_celsius, ntc_buffer, sizeof(ntc_buffer));

    self->print("POT_RAW:%u,POT_C:%s,NTC_RAW:%u,NTC_C:%s\r\n", reading->pot_raw, pot_celsius_str, reading->ntc_raw, ntc_celsius_str);
}

static inline enum SystemFsmStatus prv_nested_status(enum SystemFsmStatus base, int code) {
    /* the sub-module's code lands inside base's range: 305 is the pot model failing with code 5 */
    return (enum SystemFsmStatus)(base + code);
}
