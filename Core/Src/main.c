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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <assert.h>
#include "system-fsm-api.h"
#include "adc-filter-api.h"
#include "float-format-api.h"
#include "ntc-api.h"
#include "cli-api.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HOW_MANY_BYTES_TO_READ (1)
#define DMA_BUFFER_SIZE (2)
#define CLI_LINE_MAX_LENGTH (64)
#define FLOAT_STRING_MAX_LENGTH (10)

#define NTC_R_PULLUP (18000.0f)
#define NTC_R0 (8755.0f)
#define NTC_T0_KELVIN (298.15f)
#define NTC_BETA (3950.0f)

#define POT_R_PULLUP (5000.0f)
#define POT_R0 (5000.0f)
#define POT_T0_KELVIN (298.15f)
#define POT_BETA (3950.0f)

/* TODO: validate the ADC itself against VREFINT */
#define NTC_RAW_MIN (50U)
#define NTC_RAW_MAX (4045U)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t rx_byte;
static volatile uint32_t adc_conversion_count;

static struct Cli cli;

/* dma_buffer[0] = POT (channel 0); dma_buffer[1] = NTC (channel 1) */
static uint16_t dma_buffer[DMA_BUFFER_SIZE];

/*! Potentiometer instances */
static struct AdcFilter pot_filter;
static struct Ntc pot;

/*! NTC instances */
static struct AdcFilter ntc_filter;
static struct Ntc ntc;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void prv_state_cli(void);
static void prv_state_streaming(void);

static enum CommandHandlerStatus prv_command_help(int argc, char *argv[]);
static enum CommandHandlerStatus prv_command_raw(int argc, char *argv[]);
static enum CommandHandlerStatus prv_command_avg(int argc, char *argv[]);
static enum CommandHandlerStatus prv_command_noise(int argc, char *argv[]);

static enum SystemFsmFatalErrorCode prv_port_self_test(void);

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
    /* USER CODE BEGIN 2 */
    static const struct SystemFsmPort system_fsm_port = {
        .self_test = prv_port_self_test,
    };

    static struct SystemFsm system_fsm = {
        .port = &system_fsm_port,
        .get_tick = HAL_GetTick,
    };

    enum SystemFsmStatus fsm_status = system_fsm_init(&system_fsm);
    if (fsm_status != SYSTEM_FSM_OK) {
        printf("[FATAL] system_fsm_init failed: %d\r\n", fsm_status);
        Error_Handler();
    }

    /* static: cli keeps pointers to both for the whole program lifetime */
    static char cli_storage[CLI_LINE_MAX_LENGTH];
    static const struct Command cli_commands[] = {
        { "help", prv_command_help },
        { "raw", prv_command_raw },
        { "avg", prv_command_avg },
        { "noise", prv_command_noise },
    };
    const size_t cli_commands_length = sizeof(cli_commands) / sizeof(cli_commands[0]);
    /* TODO: handle initialization errors */
    cli_init(&cli, cli_storage, sizeof(cli_storage), cli_commands, cli_commands_length);

    /* TODO: handle initialization errors */
    adc_filter_init(&pot_filter);
    ntc_init(&pot, POT_R_PULLUP, POT_R0, POT_T0_KELVIN, POT_BETA);
    adc_filter_init(&ntc_filter);
    ntc_init(&ntc, NTC_R_PULLUP, NTC_R0, NTC_T0_KELVIN, NTC_BETA);

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        fsm_status = system_fsm_run(&system_fsm);
        assert(fsm_status == SYSTEM_FSM_OK);
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
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

static void prv_state_cli(void) {
    if (cli.status == CLI_FEED_EMPTY) {
        printf("> ");
        fflush(stdout);
        cli_set_status_unchanged(&cli);
    } else if (cli.status == CLI_FEED_UPDATED) {
        printf("\r\n> %s", cli_get_command(&cli));
        fflush(stdout);
        cli_set_status_unchanged(&cli);
        return;
    }
    if (cli.status != CLI_FEED_EXECUTABLE) {
        assert(cli.status != CLI_FEED_SELF_IS_NULL);
        return;
    }
    printf("\r\n");
    const enum CommandExecutorStatus exec_status = cli_execute(&cli);
    switch (exec_status) {
        case COMMAND_EXECUTOR_OK:
            return;
        case COMMAND_EXECUTOR_EMPTY_LINE:
            printf("[ERROR] empty line\r\n");
            return;
        case COMMAND_EXECUTOR_TOO_MANY_ARGS:
            printf("[ERROR] too many args\r\n");
            return;
        case COMMAND_EXECUTOR_COMMAND_NOT_FOUND:
            printf("[ERROR] command not found\r\n");
            return;
        case COMMAND_EXECUTOR_INVALID_ARGS:
            /* The handler has already reported the error to the user */
            return;
        default:
            printf("[FATAL] cli_execute: should not be here. Error code: %d\r\n", exec_status);
            return;
    }
}

static void prv_state_streaming(void) {
    const uint16_t pot_raw = adc_filter_apply(&pot_filter, dma_buffer[0]);
    char pot_celsius[FLOAT_STRING_MAX_LENGTH];
    float_to_string(ntc_raw_adc_to_celsius(&pot, pot_raw), pot_celsius, sizeof(pot_celsius));

    const uint16_t ntc_raw = adc_filter_apply(&ntc_filter, dma_buffer[1]);
    char ntc_celsius[FLOAT_STRING_MAX_LENGTH];
    float_to_string(ntc_raw_adc_to_celsius(&ntc, ntc_raw), ntc_celsius, sizeof(ntc_celsius));

    printf("POT_RAW:%d,POT_C:%s,NTC_RAW:%d,NTC_C:%s\r\n", pot_raw, pot_celsius, ntc_raw, ntc_celsius);
    HAL_Delay(100);
}

static enum CommandHandlerStatus prv_command_help(int argc, char *argv[]) {
    printf("Available commands:\r\n");
    printf("  help  - Show this help message\r\n");
    printf("  raw   - Remove all filters\r\n");
    printf("  avg   - Add the 150-sample moving average filter\r\n");
    printf("  noise - Add random noise injection\r\n");
    printf("Filters stack: avg and noise can be active together.\r\n");
    printf("Press the user button to go back to streaming.\r\n");
    return COMMAND_HANDLER_OK;
}

static enum CommandHandlerStatus prv_command_raw(int argc, char *argv[]) {
    adc_filter_handle_event(&pot_filter, ADC_FILTER_EVENT_CLEAR);
    adc_filter_handle_event(&ntc_filter, ADC_FILTER_EVENT_CLEAR);
    printf("removed all the filters applied\r\n");
    return COMMAND_HANDLER_OK;
}

static enum CommandHandlerStatus prv_command_avg(int argc, char *argv[]) {
    adc_filter_handle_event(&pot_filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    adc_filter_handle_event(&ntc_filter, ADC_FILTER_EVENT_ADD_AVERAGE);
    printf("moving average filtering enabled\r\n");
    return COMMAND_HANDLER_OK;
}

static enum CommandHandlerStatus prv_command_noise(int argc, char *argv[]) {
    adc_filter_handle_event(&pot_filter, ADC_FILTER_EVENT_ADD_NOISE);
    adc_filter_handle_event(&ntc_filter, ADC_FILTER_EVENT_ADD_NOISE);
    printf("noise injection enabled\r\n");
    return COMMAND_HANDLER_OK;
}

static enum SystemFsmFatalErrorCode prv_port_self_test(void) {
    const HAL_StatusTypeDef uart_result = HAL_UART_Receive_IT(&huart2, &rx_byte, HOW_MANY_BYTES_TO_READ);
    if (uart_result != HAL_OK) {
        return SYSTEM_FSM_FATAL_ERROR_SERIAL_START_FAILED;
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

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin != BTN_Pin) {
        return;
    }
    /* TODO: report the press to the system FSM */
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(&huart2, &rx_byte, HOW_MANY_BYTES_TO_READ);
    if (huart != &huart2 /*|| current_state != &prv_state_cli*/) {
        return;
    }
    cli_feed(&cli, rx_byte);
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
