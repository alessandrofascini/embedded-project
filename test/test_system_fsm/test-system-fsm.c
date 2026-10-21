/*!
 * \file test-system-fsm.c
 * \author Alessandro Fascini
 *
 * \brief System FSM test file.
 */

#include "unity.h"
#include "system-fsm-api.h"
#include "ntc-api.h"
#include "fff.h"

DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(enum SystemFsmFatalErrorCode, fake_self_test);
FAKE_VALUE_FUNC(uint32_t, fake_get_tick);
FAKE_VOID_FUNC(fake_cli_print);
FAKE_VOID_FUNC(fake_cli_reset);
FAKE_VOID_FUNC(fake_led_set_blink, enum SystemFsmLed, uint16_t, uint8_t);
FAKE_VOID_FUNC(fake_led_set_breathing, enum SystemFsmLed, uint16_t);
FAKE_VOID_FUNC(fake_led_set_duty, enum SystemFsmLed, uint8_t);
FAKE_VOID_FUNC(fake_read_raw, uint16_t *, uint16_t *);
FAKE_VOID_FUNC_VARARG(fake_print, const char *, ...);
FAKE_VOID_FUNC(fake_reset_mcu);

static const struct SystemFsmCli test_cli = {
    .print = fake_cli_print,
    .reset = fake_cli_reset,
};

static const struct SystemFsmLeds test_leds = {
    .set_blink = fake_led_set_blink,
    .set_breathing = fake_led_set_breathing,
    .set_duty = fake_led_set_duty,
};

void setUp(void) {
}

void tearDown(void) {
}

void testSystemFsmInit(void) {
    struct SystemFsm fsm = { 0 };

    TEST_ASSERT_EQUAL(SYSTEM_FSM_SELF_IS_NULL, system_fsm_init(NULL, NULL));
    TEST_ASSERT_EQUAL(SYSTEM_FSM_SELF_TEST_IS_NULL, system_fsm_init(&fsm, NULL));

    /* each case sets the callback the previous one reported missing */
    fsm.self_test = fake_self_test;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_CLI_IS_NULL, system_fsm_init(&fsm, NULL));
    fsm.cli = &test_cli;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_LEDS_IS_NULL, system_fsm_init(&fsm, NULL));
    fsm.leds = &test_leds;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_READ_RAW_IS_NULL, system_fsm_init(&fsm, NULL));
    fsm.read_raw = fake_read_raw;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_PRINT_IS_NULL, system_fsm_init(&fsm, NULL));
    fsm.print = fake_print;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_RESET_MCU_IS_NULL, system_fsm_init(&fsm, NULL));
    fsm.reset_mcu = fake_reset_mcu;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_GET_TICK_IS_NULL, system_fsm_init(&fsm, NULL));
    fsm.get_tick = fake_get_tick;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_CONFIG_IS_NULL, system_fsm_init(&fsm, NULL));

    const struct NtcParams params = {
        .r_pullup = 10000.0f,
        .r0_ohms = 10000.0f,
        .t0_kelvin = 298.15f,
        .beta = 3950.0f,
    };
    struct SystemFsmConfig config = { .pot = params, .ntc = params };

    /* a rejected model is reported as its range base + its own error code */
    config.pot.beta = 0.0f;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_POT_MODEL_INIT_FAILED + NTC_INVALID_BETA, system_fsm_init(&fsm, &config));
    config.pot = params;
    config.ntc.r0_ohms = 0.0f;
    TEST_ASSERT_EQUAL(SYSTEM_FSM_NTC_MODEL_INIT_FAILED + NTC_INVALID_R0, system_fsm_init(&fsm, &config));
    config.ntc = params;

    /* a valid init starts in POST with the CLI disabled; the self-test waits for the first run */
    TEST_ASSERT_EQUAL(SYSTEM_FSM_OK, system_fsm_init(&fsm, &config));
    TEST_ASSERT_EQUAL(SYSTEM_FSM_STATE_POST, system_fsm_get_state(&fsm));
    TEST_ASSERT_FALSE(system_fsm_is_cli_enabled(&fsm));
    TEST_ASSERT_EQUAL(0, fake_self_test_fake.call_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testSystemFsmInit);
    return UNITY_END();
}
