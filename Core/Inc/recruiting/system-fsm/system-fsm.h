/*!
 * \file system-fsm.h
 * \author Alessandro Fascini
 * \brief Types of the system FSM: the SystemFsm instance, its states, triggers,
 *        status codes and the hardware callbacks the application plugs in.
 */

#ifndef RECRUITING_SYSTEM_FSM_H
#define RECRUITING_SYSTEM_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include "eagletrt-fsm.h"
#include "adc-filter.h"
#include "ntc.h"

/*!
 * \brief Reasons for entering the fatal state, printed by the fatal report.
 */
enum SystemFsmFatalErrorCode {
    SYSTEM_FSM_FATAL_ERROR_NONE,                /*!< No error: the self-test passed, or no error is recorded yet. */
    SYSTEM_FSM_FATAL_ERROR_UNKNOWN,             /*!< Unknown error; its message is also printed for a code without one. */
    SYSTEM_FSM_FATAL_ERROR_SERIAL_START_FAILED, /*!< The serial reception could not be started. */
    SYSTEM_FSM_FATAL_ERROR_SENSOR_START_FAILED, /*!< The sensor acquisition could not be started. */
    SYSTEM_FSM_FATAL_ERROR_SENSOR_TIMEOUT,      /*!< No sensor conversion completed within the timeout. */
    SYSTEM_FSM_FATAL_ERROR_NTC_OUT_OF_RANGE,    /*!< The NTC reading is near a rail: open or shorted sensor. */
    SYSTEM_FSM_FATAL_ERROR_LED_START_FAILED,    /*!< The LED timers could not be started. */
    SYSTEM_FSM_FATAL_ERROR_WARNING_TIMEOUT,     /*!< The warning state lasted 10 s. */
    SYSTEM_FSM_FATAL_ERROR_COUNT,               /*!< Number of error codes, not a valid code. */
};

/*!
 * \brief Power-on self-test, run once in the POST state.
 * \return The first failure found, or SYSTEM_FSM_FATAL_ERROR_NONE if everything started.
 */
typedef enum SystemFsmFatalErrorCode (*system_fsm_self_test)(void);

/*!
 * \brief Monotonic millisecond tick; it may wrap around.
 * \return The current tick in milliseconds.
 */
typedef uint32_t (*system_fsm_get_tick)(void);

/*!
 * \brief Serve the CLI: refresh the prompt and run a completed command.
 *
 * Called on every run of the waiting and paused states.
 */
typedef void (*system_fsm_print_cli)(void);

/*!
 * \brief Discard any partially typed command line.
 *
 * Called just before the CLI is enabled.
 */
typedef void (*system_fsm_reset_cli)(void);

/*!
 * \brief LEDs driven by the FSM; the application maps each one to its hardware.
 */
enum SystemFsmLed {
    SYSTEM_FSM_LED_BOARD,    /*!< LED on the board. */
    SYSTEM_FSM_LED_EXTERNAL, /*!< External LED. */
    SYSTEM_FSM_LED_COUNT,    /*!< Number of LEDs, not a valid LED. */
};

/*!
 * \brief Blink an LED.
 * \param led          The LED to drive.
 * \param period_ms    Blink period in milliseconds.
 * \param duty_percent Fraction of the period the LED is on, 0 to 100.
 */
typedef void (*system_fsm_led_set_blink)(enum SystemFsmLed led, uint16_t period_ms, uint8_t duty_percent);

/*!
 * \brief Fade an LED in and out continuously.
 * \param led       The LED to drive.
 * \param period_ms Duration of a full fade-in and fade-out cycle in milliseconds.
 */
typedef void (*system_fsm_led_set_breathing)(enum SystemFsmLed led, uint16_t period_ms);

/*!
 * \brief Drive an LED at a fixed brightness, stopping any blink or breathing.
 * \param led          The LED to drive.
 * \param duty_percent Brightness, 0 (off) to 100.
 */
typedef void (*system_fsm_led_set_duty)(enum SystemFsmLed led, uint8_t duty_percent);

/*!
 * \brief Read the latest raw ADC conversions; the FSM filters and converts them.
 * \param pot_raw Filled with the potentiometer ADC value; never NULL.
 * \param ntc_raw Filled with the NTC ADC value; never NULL.
 */
typedef void (*system_fsm_read_raw)(uint16_t *pot_raw, uint16_t *ntc_raw);

/*!
 * \brief Board parameters, read once by system_fsm_init().
 */
struct SystemFsmConfig {
    struct NtcParams pot; /*!< Circuit of the potentiometer, modelled as an NTC. */
    struct NtcParams ntc; /*!< Circuit of the NTC. */
};

/*!
 * \brief printf-like output used for the readings, the warning and the fatal report.
 * \param format printf-like format string, followed by its arguments.
 */
typedef void (*system_fsm_print)(const char *format, ...);

/*!
 * \brief Reset the MCU; not expected to return.
 */
typedef void (*system_fsm_reset_mcu)(void);

/*!
 * \brief Result codes for the system FSM functions.
 *
 * \details A sub-module that fails inside system_fsm_init() is reported as
 * the base of its range plus the sub-module's own error code, e.g. 305 is
 * SYSTEM_FSM_POT_MODEL_INIT_FAILED + NTC_INVALID_BETA.
 */
enum SystemFsmStatus {
    SYSTEM_FSM_OK,                           /*!< Operation succeeded. */
    SYSTEM_FSM_SELF_IS_NULL,                 /*!< self is NULL. */
    SYSTEM_FSM_INVALID_MACHINE,              /*!< The underlying FSM rejected the state table. */
    SYSTEM_FSM_INVALID_TRANSITION,           /*!< The FSM is not initialized, or the requested state is not reachable from the current one. */
    SYSTEM_FSM_SELF_TEST_IS_NULL,            /*!< self->self_test is NULL. */
    SYSTEM_FSM_GET_TICK_IS_NULL,             /*!< self->get_tick is NULL. */
    SYSTEM_FSM_CLI_IS_NULL,                  /*!< self->cli is NULL. */
    SYSTEM_FSM_LEDS_IS_NULL,                 /*!< self->leds is NULL. */
    SYSTEM_FSM_PRINT_IS_NULL,                /*!< self->print is NULL. */
    SYSTEM_FSM_RESET_MCU_IS_NULL,            /*!< self->reset_mcu is NULL. */
    SYSTEM_FSM_READ_RAW_IS_NULL,             /*!< self->read_raw is NULL. */
    SYSTEM_FSM_CONFIG_IS_NULL,               /*!< The config passed to system_fsm_init() is NULL. */
    SYSTEM_FSM_POT_FILTER_INIT_FAILED = 100, /*!< 100 + enum AdcFilterError of the potentiometer filter. */
    SYSTEM_FSM_NTC_FILTER_INIT_FAILED = 200, /*!< 200 + enum AdcFilterError of the NTC filter. */
    SYSTEM_FSM_POT_MODEL_INIT_FAILED = 300,  /*!< 300 + enum NtcError of the potentiometer model. */
    SYSTEM_FSM_NTC_MODEL_INIT_FAILED = 400,  /*!< 400 + enum NtcError of the NTC model. */
};

/*!
 * \brief States of the system FSM.
 */
enum SystemFsmState {
    SYSTEM_FSM_STATE_POST,      /*!< Initial state: runs the self-test, then goes to waiting or fatal. */
    SYSTEM_FSM_STATE_WAITING,   /*!< Idle with the CLI enabled, waiting for a click or the run command. */
    SYSTEM_FSM_STATE_LISTENING, /*!< Streams the readings and drives the external LED from NTC - POT. */
    SYSTEM_FSM_STATE_WARNING,   /*!< NTC - POT stayed above the threshold for 5 s; left with a 2 s hold, fatal after 10 s. */
    SYSTEM_FSM_STATE_PAUSED,    /*!< Listening suspended with the CLI enabled. */
    SYSTEM_FSM_STATE_FATAL,     /*!< Terminal: reports the error until a click resets the MCU. */
    SYSTEM_FSM_STATE_COUNT,     /*!< Number of states, not a valid state. */
};

/*!
 * \brief External events posted to the FSM with system_fsm_post_trigger().
 */
enum SystemFsmTrigger {
    SYSTEM_FSM_TRIGGER_BUTTON_CLICK, /*!< Button pressed: posted at the start of every press, long ones included. */
    SYSTEM_FSM_TRIGGER_RUN_COMMAND,  /*!< The run command was entered on the CLI. */
    SYSTEM_FSM_TRIGGER_BUTTON_HOLD,  /*!< Button held for 2 s. */
    SYSTEM_FSM_TRIGGER_COUNT,        /*!< Number of triggers, not a valid trigger. */
};

/*!
 * \brief CLI callbacks; the FSM decides when the CLI is enabled.
 */
struct SystemFsmCli {
    system_fsm_print_cli print; /*!< Serve the CLI while it is enabled. */
    system_fsm_reset_cli reset; /*!< Clear the command line before enabling the CLI. */
};

/*!
 * \brief LED callbacks; the FSM chooses the pattern of each LED per state.
 */
struct SystemFsmLeds {
    system_fsm_led_set_blink set_blink;         /*!< Blink an LED. */
    system_fsm_led_set_breathing set_breathing; /*!< Fade an LED in and out. */
    system_fsm_led_set_duty set_duty;           /*!< Drive an LED at a fixed brightness. */
};

/*!
 * \brief Runtime data of the fatal state.
 */
struct SystemFsmFatal {
    enum SystemFsmFatalErrorCode error; /*!< First error that led to the fatal state: the root cause. */
    uint32_t last_message_ms;           /*!< Tick of the last error report; 0 before the first one. */
};

/*!
 * \brief Runtime data of the listening state.
 */
struct SystemFsmListening {
    uint32_t last_sample_ms; /*!< Tick of the last sensor sample, or of entering the state before the first one. */
    bool is_above;           /*!< NTC - POT was above the threshold at the last sample; reset on entry. */
    uint32_t above_since_ms; /*!< Tick at which NTC - POT went above the threshold; valid only while is_above. */
};

/*!
 * \brief Runtime data of the warning state.
 */
struct SystemFsmWarning {
    uint32_t entered_ms;      /*!< Tick at which the warning state was entered, for the 10 s timeout. */
    uint32_t last_sample_ms;  /*!< Tick of the last sensor sample, or of entering the state before the first one. */
    uint32_t last_message_ms; /*!< Tick of the last WARNING message; set one period back on entry, so the first goes out at once. */
};

/*!
 * \brief System FSM instance.
 *
 * The application fills in the callbacks and calls system_fsm_init(); the
 * remaining fields are owned by the FSM.
 */
struct SystemFsm {
    struct FSMHandler handler;           /*!< Underlying generic FSM; owned by the FSM. */
    system_fsm_self_test self_test;      /*!< Power-on self-test. */
    const struct SystemFsmCli *cli;      /*!< CLI callbacks. */
    const struct SystemFsmLeds *leds;    /*!< LED callbacks. */
    system_fsm_read_raw read_raw;        /*!< Raw ADC sampling. */
    system_fsm_print print;              /*!< printf-like output, e.g. serial_print. */
    system_fsm_reset_mcu reset_mcu;      /*!< MCU reset, used to leave the fatal state. */
    struct SystemFsmListening listening; /*!< Listening state data; owned by the FSM. */
    struct SystemFsmWarning warning;     /*!< Warning state data; owned by the FSM. */
    struct SystemFsmFatal fatal;         /*!< Fatal state data; owned by the FSM. */
    system_fsm_get_tick get_tick;        /*!< Millisecond tick source. */
    struct AdcFilter pot_filter;         /*!< Potentiometer filter; set up by system_fsm_init(), the application may switch its filters with adc_filter_handle_event(). */
    struct Ntc pot;                      /*!< Potentiometer model; owned by the FSM, set up by system_fsm_init(). */
    struct AdcFilter ntc_filter;         /*!< NTC filter; set up by system_fsm_init(), the application may switch its filters with adc_filter_handle_event(). */
    struct Ntc ntc;                      /*!< NTC model; owned by the FSM, set up by system_fsm_init(). */
};

#endif
