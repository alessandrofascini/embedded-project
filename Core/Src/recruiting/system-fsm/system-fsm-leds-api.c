/*!
 * \file system-fsm-leds-api.c
 * \author Alessandro Fascini
 * \brief LED patterns of the system FSM: the pattern of each LED in each state.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include "system-fsm-leds.h"

/*!
 * \brief How an LED is driven.
 */
enum SystemFsmLedMode {
    SYSTEM_FSM_LED_MODE_BLINK,     /*!< On for duty_percent of each period. */
    SYSTEM_FSM_LED_MODE_BREATHING, /*!< Fades in and out over each period. */
    SYSTEM_FSM_LED_MODE_OFF,       /*!< Off. */
};

/*!
 * \brief The pattern of one LED in one state.
 */
struct SystemFsmLedPattern {
    enum SystemFsmLedMode mode; /*!< How the LED is driven. */
    uint16_t period_ms;         /*!< Blink or breathing period; unused when off. */
    uint8_t duty_percent;       /*!< On fraction of the period; blink only. */
};

/*! \brief Initializer for a blinking LED: period in ms, duty in percent. */
#define SYSTEM_FSM_LED_BLINK(period, duty) \
    { .mode = SYSTEM_FSM_LED_MODE_BLINK, .period_ms = (period), .duty_percent = (duty) }
/*! \brief Initializer for a breathing LED: period in ms. */
#define SYSTEM_FSM_LED_BREATHING(period) \
    { .mode = SYSTEM_FSM_LED_MODE_BREATHING, .period_ms = (period) }
/*! \brief Initializer for an LED that is off. */
#define SYSTEM_FSM_LED_OFF() \
    { .mode = SYSTEM_FSM_LED_MODE_OFF }

static const struct SystemFsmLedPattern prv_led_patterns[SYSTEM_FSM_STATE_COUNT][SYSTEM_FSM_LED_COUNT] = {
    [SYSTEM_FSM_STATE_WAITING] = {
        [SYSTEM_FSM_LED_BOARD] = SYSTEM_FSM_LED_BLINK(500U, 15U),
        [SYSTEM_FSM_LED_EXTERNAL] = SYSTEM_FSM_LED_BREATHING(2000U),
    },
    [SYSTEM_FSM_STATE_LISTENING] = {
        [SYSTEM_FSM_LED_BOARD] = SYSTEM_FSM_LED_BLINK(200U, 80U),
        [SYSTEM_FSM_LED_EXTERNAL] = SYSTEM_FSM_LED_OFF(), /* driven by the listening routine */
    },
    [SYSTEM_FSM_STATE_PAUSED] = {
        [SYSTEM_FSM_LED_BOARD] = SYSTEM_FSM_LED_BLINK(2000U, 50U),
        [SYSTEM_FSM_LED_EXTERNAL] = SYSTEM_FSM_LED_BREATHING(2000U), /* same as waiting */
    },
    [SYSTEM_FSM_STATE_WARNING] = {
        [SYSTEM_FSM_LED_BOARD] = SYSTEM_FSM_LED_OFF(),
        [SYSTEM_FSM_LED_EXTERNAL] = SYSTEM_FSM_LED_OFF(),
    },
    [SYSTEM_FSM_STATE_FATAL] = {
        [SYSTEM_FSM_LED_BOARD] = SYSTEM_FSM_LED_BLINK(40U, 50U),
        [SYSTEM_FSM_LED_EXTERNAL] = SYSTEM_FSM_LED_OFF(),
    },
};

static inline void prv_led_shut_down(const struct SystemFsmLeds *leds, enum SystemFsmLed led) {
    leds->set_duty(led, 0U);
}

static inline void prv_set_led(const struct SystemFsmLeds *leds, enum SystemFsmLed led, const struct SystemFsmLedPattern *pattern) {
    switch (pattern->mode) {
        case SYSTEM_FSM_LED_MODE_BLINK:
            leds->set_blink(led, pattern->period_ms, pattern->duty_percent);
            break;
        case SYSTEM_FSM_LED_MODE_BREATHING:
            leds->set_breathing(led, pattern->period_ms);
            break;
        case SYSTEM_FSM_LED_MODE_OFF:
            prv_led_shut_down(leds, led);
            break;
        default:
            /* TODO: handle error */
            assert(false);
            break;
    }
}

void int_system_fsm_leds_apply(const struct SystemFsmLeds *leds, enum SystemFsmState state) {
    assert(leds != NULL);
    assert(state < SYSTEM_FSM_STATE_COUNT);
    for (enum SystemFsmLed led = 0; led < SYSTEM_FSM_LED_COUNT; led++) {
        prv_set_led(leds, led, &prv_led_patterns[state][led]);
    }
}
