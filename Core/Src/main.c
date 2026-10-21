/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <assert.h>
#include "system-fsm-api.h"
#include "adc-filter-api.h"
#include "ntc-api.h"
#include "cli-api.h"
#include "serial-print-api.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HOW_MANY_BYTES_TO_READ (1)
#define DMA_BUFFER_SIZE (2)
#define CLI_LINE_MAX_LENGTH (64)

/* TODO: validate the ADC itself against VREFINT */
#define NTC_RAW_MIN (50U)
#define NTC_RAW_MAX (4045U)

#define LED_BLINK_TICK_HZ (1000U) /* blink timebase: 1 tick = 1 ms */
#define LED_DUTY_MAX_PERCENT (100U)
#define LED_PWM_TICK_HZ (1000000U)   /* breathing timebase: 1 tick = 1 us */
#define LED_PWM_PERIOD_TICKS (1000U) /* 1 kHz carrier: one update interrupt per millisecond */
/* the breathing advances once per carrier period (one update interrupt) */
#define LED_BREATHING_STEP_MS ((LED_PWM_PERIOD_TICKS * 1000U) / LED_PWM_TICK_HZ)
static_assert(LED_BREATHING_STEP_MS >= 1U, "breathing step must be at least 1 ms: lower the PWM carrier frequency");

#define BUTTON_HOLD_MS (2000U) /* a press this long is a long press (leaves the warning state) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t rx_byte;
static volatile uint32_t adc_conversion_count;

static struct SystemFsm system_fsm;

static struct Cli cli;

/* dma_buffer[0] = POT (channel 0); dma_buffer[1] = NTC (channel 1) */
static uint16_t dma_buffer[DMA_BUFFER_SIZE];

/*! Potentiometer instances */
static struct AdcFilter pot_filter;
static struct Ntc pot;

/*! NTC instances */
static struct AdcFilter ntc_filter;
static struct Ntc ntc;

static const struct SystemFsmConfig main_fsm_config = {
    .pot = {
        .r_pullup = 5000.0f,
        .r0_ohms = 5000.0f,
        .t0_kelvin = 298.15f,
        .beta = 3950.0f,
    },
    .ntc = {
        .r_pullup = 18000.0f,
        .r0_ohms = 8755.0f,
        .t0_kelvin = 298.15f,
        .beta = 3950.0f,
    },
};

/* hardware behind each SystemFsmLed: the FSM only knows the enum */
struct LedHardware {
    TIM_HandleTypeDef *timer;
    uint32_t channel;
};

static const struct LedHardware main_leds[SYSTEM_FSM_LED_COUNT] = {
    [SYSTEM_FSM_LED_BOARD] = { .timer = &htim1, .channel = TIM_CHANNEL_1 },
    [SYSTEM_FSM_LED_EXTERNAL] = { .timer = &htim14, .channel = TIM_CHANNEL_1 },
};

/* breathing progress of each LED, advanced by its timer update interrupt */
struct LedBreathing {
    volatile bool active;
    volatile uint16_t period_ms;
    volatile uint16_t phase_ms;
};

static struct LedBreathing main_breathing[SYSTEM_FSM_LED_COUNT];

/* long press detection, polled from the main loop only: no volatile needed */
struct ButtonHold {
    bool is_held;
    bool has_fired; /* one event per press, however long it is held */
    uint32_t held_since_ms;
};

static struct ButtonHold main_button_hold;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void prv_serial_write_char(char character);

static void prv_cli_print(void);
static void prv_cli_reset(void);

static uint32_t prv_min(uint32_t a, uint32_t b);

static void prv_led_set_blink(enum SystemFsmLed led, uint16_t period_ms, uint8_t duty_percent);
static void prv_led_set_breathing(enum SystemFsmLed led, uint16_t period_ms);
static void prv_led_set_duty(enum SystemFsmLed led, uint8_t duty_percent);
static inline void prv_led_stop_breathing(enum SystemFsmLed led);
static void prv_led_breathing_step(enum SystemFsmLed led);

static enum CommandHandlerStatus prv_command_help(int argc, char *argv[]);
static enum CommandHandlerStatus prv_command_raw(int argc, char *argv[]);
static enum CommandHandlerStatus prv_command_avg(int argc, char *argv[]);
static enum CommandHandlerStatus prv_command_noise(int argc, char *argv[]);
static enum CommandHandlerStatus prv_command_run(int argc, char *argv[]);

static enum SystemFsmFatalErrorCode prv_port_self_test(void);
static void prv_port_read_sensors(struct SystemFsmReading *reading);

static inline bool prv_button_hold_update(bool is_pressed, uint32_t now_ms);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_TIM1_Init();
    MX_TIM14_Init();
    /* USER CODE BEGIN 2 */
    /* first: everything below may print */
    serial_print_init(prv_serial_write_char);

    static char cli_storage[CLI_LINE_MAX_LENGTH];
    static const struct Command cli_commands[] = {
        { "help", prv_command_help },
        { "raw", prv_command_raw },
        { "avg", prv_command_avg },
        { "noise", prv_command_noise },
        { "run", prv_command_run },
    };
    const size_t cli_commands_length = sizeof(cli_commands) / sizeof(cli_commands[0]);

    const enum CliInitStatus cli_init_result = cli_init(&cli, cli_storage, sizeof(cli_storage), cli_commands, cli_commands_length);
    if (cli_init_result != CLI_INIT_OK) {
        serial_print("[FATAL] cli_init failed: %d\r\n", cli_init_result);
        Error_Handler();
    }

    /* FSM - initialization */
    static const struct SystemFsmCli system_fsm_cli = {
        .print = prv_cli_print,
        .reset = prv_cli_reset,
    };

    static const struct SystemFsmLeds system_fsm_leds = {
        .set_blink = prv_led_set_blink,
        .set_breathing = prv_led_set_breathing,
        .set_duty = prv_led_set_duty,
    };

    system_fsm = (struct SystemFsm){
        .cli = &system_fsm_cli,
        .leds = &system_fsm_leds,
        .self_test = prv_port_self_test,
        .read_sensors = prv_port_read_sensors,
        .print = serial_print,
        .reset_mcu = NVIC_SystemReset,
        .get_tick = HAL_GetTick,
    };

    enum SystemFsmStatus fsm_status = system_fsm_init(&system_fsm, &main_fsm_config);
    if (fsm_status != SYSTEM_FSM_OK) {
        serial_print("[FATAL] system_fsm_init failed: %d\r\n", fsm_status);
        Error_Handler();
    }

    /* TODO: handle initialization errors */
    adc_filter_init(&pot_filter);
    ntc_init(&pot, &main_fsm_config.pot);
    adc_filter_init(&ntc_filter);
    ntc_init(&ntc, &main_fsm_config.ntc);

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        fsm_status = system_fsm_run(&system_fsm);
        assert(fsm_status == SYSTEM_FSM_OK);

        /* the button pulls the pin low while pressed */
        const bool is_button_pressed = HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET;
        if (prv_button_hold_update(is_button_pressed, HAL_GetTick())) {
            system_fsm_post_trigger(&system_fsm, SYSTEM_FSM_TRIGGER_BUTTON_HOLD);
        }
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
  */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
static void prv_serial_write_char(char character) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&character, 1, HAL_MAX_DELAY);
}

/* replaces newlib's version, which pulls in fiprintf and with it the whole stdio */
void __assert_func(const char *file, int line, const char *function, const char *expression) {
    serial_print("[ASSERT] %s:%d: %s\r\n", file, line, expression);
    Error_Handler();
    /* Error_Handler never returns, but it is not declared noreturn as assert.h requires here */
    while (1) {
    }
}

static void prv_cli_print(void) {
    if (cli.status == CLI_FEED_EMPTY) {
        serial_print("> ");
        cli_set_status_unchanged(&cli);
    } else if (cli.status == CLI_FEED_UPDATED) {
        serial_print("\r\n> %s", cli_get_command(&cli));
        cli_set_status_unchanged(&cli);
        return;
    }
    if (cli.status != CLI_FEED_EXECUTABLE) {
        assert(cli.status != CLI_FEED_SELF_IS_NULL);
        return;
    }
    serial_print("\r\n");
    const enum CommandExecutorStatus exec_status = cli_execute(&cli);
    switch (exec_status) {
        case COMMAND_EXECUTOR_OK:
            return;
        case COMMAND_EXECUTOR_EMPTY_LINE:
            serial_print("empty line\r\n");
            return;
        case COMMAND_EXECUTOR_TOO_MANY_ARGS:
            serial_print("too many args\r\n");
            return;
        case COMMAND_EXECUTOR_COMMAND_NOT_FOUND:
            serial_print("command not found\r\n");
            return;
        case COMMAND_EXECUTOR_INVALID_ARGS:
            /* The handler has already reported the error to the user */
            return;
        default:
            serial_print("[ERROR] cli_execute: should not be here. Error code: %d\r\n", exec_status);
            return;
    }
}

static void prv_cli_reset(void) {
    cli_reset(&cli);
}

static uint32_t prv_min(uint32_t a, uint32_t b) {
    if (a < b) {
        return a;
    }
    return b;
}

static void prv_led_set_blink(enum SystemFsmLed led, uint16_t period_ms, uint8_t duty_percent) {
    /* the arguments come from FSM constants: a bad value is a programming error */
    assert(led < SYSTEM_FSM_LED_COUNT);
    assert(period_ms > 0U);
    assert(duty_percent <= LED_DUTY_MAX_PERCENT);
    if (led >= SYSTEM_FSM_LED_COUNT || period_ms == 0U) {
        return; /* without asserts: never write ARR = period_ms - 1 = 65535 */
    }
    const struct LedHardware *hw = &main_leds[led];
    const uint32_t duty = prv_min(duty_percent, LED_DUTY_MAX_PERCENT);
    prv_led_stop_breathing(led);

    /* unit of time. The 16-bit counter would overflow in ~1.4 ms at 48 MHz,
     * so the clock is divided down to LED_BLINK_TICK_HZ (1 tick = 1 ms).
     * it is divided by "register value + 1", hence the "- 1U".
     */
    const uint32_t prescaler = HAL_RCC_GetPCLK1Freq() / LED_BLINK_TICK_HZ - 1U;
    __HAL_TIM_SET_PRESCALER(hw->timer, prescaler);

    /* ticks per period: the counter goes 0..ARR, that is ARR + 1 ticks, hence the "- 1U" */
    __HAL_TIM_SET_AUTORELOAD(hw->timer, period_ms - 1U);

    /* ticks the pin stays high (the duty cycle) */
    const uint32_t duty_ticks = (uint32_t)period_ms * duty / LED_DUTY_MAX_PERCENT;
    __HAL_TIM_SET_COMPARE(hw->timer, hw->channel, duty_ticks);
    /* load the new prescaler now and restart the period: otherwise shortening ARR
     * below the current counter value makes the counter wrap around 65535 */
    hw->timer->Instance->EGR = TIM_EGR_UG;
}

static void prv_led_set_breathing(enum SystemFsmLed led, uint16_t period_ms) {
    assert(led < SYSTEM_FSM_LED_COUNT);
    assert(period_ms >= 2U);
    if (led >= SYSTEM_FSM_LED_COUNT || period_ms < 2U) {
        return; /* the triangle needs at least one millisecond up and one down */
    }
    const struct LedHardware *hw = &main_leds[led];
    struct LedBreathing *breathing = &main_breathing[led];

    /* no interrupt while the breathing state is being rewritten */
    prv_led_stop_breathing(led);
    breathing->period_ms = period_ms;
    breathing->phase_ms = 0U;

    /* fast PWM: the eye sees the duty cycle as brightness, not as blinking */
    const uint32_t prescaler = HAL_RCC_GetPCLK1Freq() / LED_PWM_TICK_HZ - 1U;
    __HAL_TIM_SET_PRESCALER(hw->timer, prescaler);
    __HAL_TIM_SET_AUTORELOAD(hw->timer, LED_PWM_PERIOD_TICKS - 1U);
    __HAL_TIM_SET_COMPARE(hw->timer, hw->channel, 0U);
    hw->timer->Instance->EGR = TIM_EGR_UG;

    /* the forced update above also raised the update flag: clear it, or the
     * interrupt would fire immediately */
    __HAL_TIM_CLEAR_FLAG(hw->timer, TIM_FLAG_UPDATE);
    breathing->active = true;
    __HAL_TIM_ENABLE_IT(hw->timer, TIM_IT_UPDATE);
}

static void prv_led_set_duty(enum SystemFsmLed led, uint8_t duty_percent) {
    assert(led < SYSTEM_FSM_LED_COUNT);
    assert(duty_percent <= LED_DUTY_MAX_PERCENT);
    if (led >= SYSTEM_FSM_LED_COUNT) {
        return;
    }
    const struct LedHardware *hw = &main_leds[led];
    const uint32_t duty = prv_min(duty_percent, LED_DUTY_MAX_PERCENT);

    /* nothing may change the duty on its own: only the next call does */
    prv_led_stop_breathing(led);

    /* same fast carrier as the breathing: the eye sees the duty cycle as brightness */
    const uint32_t prescaler = HAL_RCC_GetPCLK1Freq() / LED_PWM_TICK_HZ - 1U;
    __HAL_TIM_SET_PRESCALER(hw->timer, prescaler);
    __HAL_TIM_SET_AUTORELOAD(hw->timer, LED_PWM_PERIOD_TICKS - 1U);
    __HAL_TIM_SET_COMPARE(hw->timer, hw->channel, LED_PWM_PERIOD_TICKS * duty / LED_DUTY_MAX_PERCENT);
    hw->timer->Instance->EGR = TIM_EGR_UG;
}

static inline void prv_led_stop_breathing(enum SystemFsmLed led) {
    __HAL_TIM_DISABLE_IT(main_leds[led].timer, TIM_IT_UPDATE);
    main_breathing[led].active = false;
}

static void prv_led_breathing_step(enum SystemFsmLed led) {
    struct LedBreathing *breathing = &main_breathing[led];
    const uint16_t period_ms = breathing->period_ms;
    const uint16_t half_ms = period_ms / 2U;

    breathing->phase_ms = (breathing->phase_ms + LED_BREATHING_STEP_MS) % period_ms;
    const uint16_t phase_ms = breathing->phase_ms;

    /* triangle wave: duty rises over the first half of the period, falls over the second */
    const uint32_t rising_ms = (phase_ms < half_ms) ? phase_ms : (period_ms - phase_ms);
    const uint32_t duty_ticks = LED_PWM_PERIOD_TICKS * rising_ms / half_ms;
    __HAL_TIM_SET_COMPARE(main_leds[led].timer, main_leds[led].channel, duty_ticks);
}

static enum CommandHandlerStatus prv_command_help(int argc, char *argv[]) {
    serial_print("Available commands:\r\n");
    serial_print("  help  - Show this help message\r\n");
    serial_print("  raw   - Remove all filters\r\n");
    serial_print("  avg   - Add the 150-sample moving average filter\r\n");
    serial_print("  noise - Add random noise injection\r\n");
    serial_print("  run   - Start streaming the sensor readings\r\n");
    serial_print("Filters stack: avg and noise can be active together.\r\n");
    return COMMAND_HANDLER_OK;
}

static enum CommandHandlerStatus prv_command_raw(int argc, char *argv[]) {
    adc_filter_handle_event(&pot_filter, ADC_FILTER_EVENT_CLEAR);
    adc_filter_handle_event(&ntc_filter, ADC_FILTER_EVENT_CLEAR);
    serial_print("removed all the filters applied\r\n");
    return COMMAND_HANDLER_OK;
}

static enum CommandHandlerStatus prv_command_avg(int argc, char *argv[]) {
    adc_filter_handle_event(&pot_filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    adc_filter_handle_event(&ntc_filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    serial_print("moving average filtering enabled\r\n");
    return COMMAND_HANDLER_OK;
}

static enum CommandHandlerStatus prv_command_noise(int argc, char *argv[]) {
    adc_filter_handle_event(&pot_filter, ADC_FILTER_EVENT_ADD_NOISE);
    adc_filter_handle_event(&ntc_filter, ADC_FILTER_EVENT_ADD_NOISE);
    serial_print("noise injection enabled\r\n");
    return COMMAND_HANDLER_OK;
}

static enum CommandHandlerStatus prv_command_run(int argc, char *argv[]) {
    system_fsm_post_trigger(&system_fsm, SYSTEM_FSM_TRIGGER_RUN_COMMAND);
    return COMMAND_HANDLER_OK;
}

static enum SystemFsmFatalErrorCode prv_port_self_test(void) {
    const HAL_StatusTypeDef uart_result = HAL_UART_Receive_IT(&huart2, &rx_byte, HOW_MANY_BYTES_TO_READ);
    if (uart_result != HAL_OK) {
        return SYSTEM_FSM_FATAL_ERROR_SERIAL_START_FAILED;
    }

    for (enum SystemFsmLed led = 0; led < SYSTEM_FSM_LED_COUNT; led++) {
        if (HAL_TIM_PWM_Start(main_leds[led].timer, main_leds[led].channel) != HAL_OK) {
            return SYSTEM_FSM_FATAL_ERROR_LED_START_FAILED;
        }
    }

    const HAL_StatusTypeDef dma_result = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dma_buffer, DMA_BUFFER_SIZE);
    if (dma_result != HAL_OK) {
        return SYSTEM_FSM_FATAL_ERROR_SENSOR_START_FAILED;
    }

    uint32_t waited_ms = 0;
    while (adc_conversion_count < 1) {
        if (waited_ms >= 100) {
            return SYSTEM_FSM_FATAL_ERROR_SENSOR_TIMEOUT;
        }
        HAL_Delay(1);
        waited_ms++;
    }
    /* the potentiometer is not checked: every position, rails included, is a valid reading */
    const uint16_t ntc_raw = dma_buffer[1];
    if (ntc_raw < NTC_RAW_MIN || ntc_raw > NTC_RAW_MAX) {
        return SYSTEM_FSM_FATAL_ERROR_NTC_OUT_OF_RANGE;
    }
    return SYSTEM_FSM_FATAL_ERROR_NONE;
}

static void prv_port_read_sensors(struct SystemFsmReading *reading) {
    assert(reading != NULL);
    /* the DMA keeps the buffer fresh: take the latest conversion through the active filters */
    reading->pot_raw = adc_filter_apply(&pot_filter, dma_buffer[0]);
    reading->pot_celsius = ntc_raw_adc_to_celsius(&pot, reading->pot_raw);
    reading->ntc_raw = adc_filter_apply(&ntc_filter, dma_buffer[1]);
    reading->ntc_celsius = ntc_raw_adc_to_celsius(&ntc, reading->ntc_raw);
}

/* returns true once per press, when it has been held for BUTTON_HOLD_MS */
static inline bool prv_button_hold_update(bool is_pressed, uint32_t now_ms) {
    struct ButtonHold *hold = &main_button_hold;
    if (!is_pressed) {
        hold->is_held = false;
        hold->has_fired = false;
        return false;
    }
    if (!hold->is_held) {
        hold->is_held = true;
        hold->held_since_ms = now_ms;
        return false;
    }
    /* unsigned subtraction: still correct when the tick wraps around */
    if (hold->has_fired || (now_ms - hold->held_since_ms) < BUTTON_HOLD_MS) {
        return false;
    }
    hold->has_fired = true;
    return true;
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin != BTN_Pin) {
        return;
    }
    system_fsm_post_trigger(&system_fsm, SYSTEM_FSM_TRIGGER_BUTTON_CLICK);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(&huart2, &rx_byte, HOW_MANY_BYTES_TO_READ);
    if (huart != &huart2) {
        return;
    }
    /* TODO: think better about this part. */
    if (system_fsm_is_cli_enabled(&system_fsm)) {
        cli_feed(&cli, rx_byte);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    /* one update per PWM period (1 ms): advance the breathing of the LED on this timer */
    for (enum SystemFsmLed led = 0; led < SYSTEM_FSM_LED_COUNT; led++) {
        if (main_leds[led].timer == htim && main_breathing[led].active) {
            prv_led_breathing_step(led);
        }
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc == &hadc1) {
        adc_conversion_count++;
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
