/*!
 * \file system-fsm.h
 * \author Alessandro Fascini
 * \brief Types of the system FSM.
 */

#ifndef RECRUITING_SYSTEM_FSM_H
#define RECRUITING_SYSTEM_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include "eagletrt-fsm.h"

enum SystemFsmFatalErrorCode {
    SYSTEM_FSM_FATAL_ERROR_NONE,
    SYSTEM_FSM_FATAL_ERROR_UNKNOWN,
    SYSTEM_FSM_FATAL_ERROR_SERIAL_START_FAILED,
    SYSTEM_FSM_FATAL_ERROR_SENSOR_START_FAILED,
    SYSTEM_FSM_FATAL_ERROR_SENSOR_TIMEOUT,
    SYSTEM_FSM_FATAL_ERROR_NTC_OUT_OF_RANGE,
    SYSTEM_FSM_FATAL_ERROR_LED_START_FAILED,
    SYSTEM_FSM_FATAL_ERROR_WARNING_TIMEOUT,
    SYSTEM_FSM_FATAL_ERROR_COUNT,
};

typedef enum SystemFsmFatalErrorCode (*system_fsm_self_test)(void);
typedef uint32_t (*system_fsm_get_tick)(void);
typedef void (*system_fsm_print_cli)(void);
typedef void (*system_fsm_reset_cli)(void);

enum SystemFsmLed {
    SYSTEM_FSM_LED_BOARD,
    SYSTEM_FSM_LED_EXTERNAL,
    SYSTEM_FSM_LED_COUNT,
};

typedef void (*system_fsm_led_set_blink)(enum SystemFsmLed led, uint16_t period_ms, uint8_t duty_percent);
typedef void (*system_fsm_led_set_breathing)(enum SystemFsmLed led, uint16_t period_ms);
typedef void (*system_fsm_led_set_duty)(enum SystemFsmLed led, uint8_t duty_percent);

struct SystemFsmReading {
    uint16_t pot_raw;
    float pot_celsius;
    uint16_t ntc_raw;
    float ntc_celsius;
};

typedef void (*system_fsm_read_sensors)(struct SystemFsmReading *reading);
/* same signature as printf, so printf itself can be plugged in */
typedef int (*system_fsm_print)(const char *format, ...);
typedef void (*system_fsm_reset_mcu)(void);

enum SystemFsmStatus {
    SYSTEM_FSM_OK,
    SYSTEM_FSM_SELF_IS_NULL,
    SYSTEM_FSM_INVALID_MACHINE,
    SYSTEM_FSM_INVALID_TRANSITION,
    SYSTEM_FSM_SELF_TEST_IS_NULL,
    SYSTEM_FSM_GET_TICK_IS_NULL,
    SYSTEM_FSM_CLI_IS_NULL,
    SYSTEM_FSM_LEDS_IS_NULL,
    SYSTEM_FSM_READ_SENSORS_IS_NULL,
    SYSTEM_FSM_PRINT_IS_NULL,
    SYSTEM_FSM_RESET_MCU_IS_NULL,
};

enum SystemFsmState {
    SYSTEM_FSM_STATE_POST,
    SYSTEM_FSM_STATE_WAITING,
    SYSTEM_FSM_STATE_LISTENING,
    SYSTEM_FSM_STATE_WARNING,
    SYSTEM_FSM_STATE_PAUSED,
    SYSTEM_FSM_STATE_FATAL,
    SYSTEM_FSM_STATE_COUNT,
    SYSTEM_FSM_STATE_UNKNOWN,
};

enum SystemFsmTrigger {
    SYSTEM_FSM_TRIGGER_BUTTON_CLICK,
    SYSTEM_FSM_TRIGGER_RUN_COMMAND,
    SYSTEM_FSM_TRIGGER_BUTTON_HOLD,
    SYSTEM_FSM_TRIGGER_COUNT,
};

struct SystemFsmCli {
    system_fsm_print_cli print;
    system_fsm_reset_cli reset;
};

struct SystemFsmLeds {
    system_fsm_led_set_blink set_blink;
    system_fsm_led_set_breathing set_breathing;
    system_fsm_led_set_duty set_duty;
};

struct SystemFsmFatal {
    enum SystemFsmFatalErrorCode error;
    uint32_t last_message_ms;
};

struct SystemFsmListening {
    uint32_t last_sample_ms;
    bool is_above;           /* NTC - POT is above the threshold */
    uint32_t above_since_ms; /* valid only while is_above */
};

struct SystemFsmWarning {
    uint32_t entered_ms;
    uint32_t last_sample_ms;
    uint32_t last_message_ms;
};

struct SystemFsm {
    struct FSMHandler handler;
    system_fsm_self_test self_test;
    const struct SystemFsmCli *cli;
    const struct SystemFsmLeds *leds;
    system_fsm_read_sensors read_sensors;
    system_fsm_print print;
    system_fsm_reset_mcu reset_mcu;
    struct SystemFsmListening listening;
    struct SystemFsmWarning warning;
    struct SystemFsmFatal fatal;
    system_fsm_get_tick get_tick;
};

#endif
