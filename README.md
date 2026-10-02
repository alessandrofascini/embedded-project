# Recruiting Firmware — NTC + Potentiometer Acquisition with CLI

Firmware for the **NUCLEO-C031C6** (STM32C031C6, Cortex-M0+) that samples an NTC
thermistor and a potentiometer via ADC/DMA, applies selectable filters, and
streams the result over the ST-LINK Virtual COM Port (USART2). A CLI mode lets
the user change the active filter at runtime. The user button (B1) toggles
between the two modes.

> **Status: work in progress.** This README is being written alongside the
> implementation. Sections marked `(TBD)` will be filled in as the
> corresponding subtask lands. See [Project status](#project-status) for what
> is done so far.

## Table of contents

- [Hardware](#hardware)
- [Building and flashing](#building-and-flashing)
- [Running tests](#running-tests) (TBD)
- [Operating modes](#operating-modes) (TBD)
- [CLI commands](#cli-commands) (TBD)
- [FSM design](#fsm-design) (TBD)
- [Filters](#filters) (TBD)
- [Serial output format](#serial-output-format) (TBD)
- [Design choices and reasoning](#design-choices-and-reasoning)
- [Project status](#project-status)

## Hardware

Board: **NUCLEO-C031C6** (MCU: STM32C031C6Tx, LQFP48, Cortex-M0+ @ 48 MHz,
HSE-derived clock).

An external breakout board provides the potentiometer and NTC thermistor,
connected via the Nucleo's Arduino-style header:

| Breakout pin | Connects to | MCU pin | Function |
|---|---|---|---|
| `3V3`  | Nucleo 3V3            | —   | Supply |
| `GND`  | Nucleo GND            | —   | Ground |
| `POT`  | Potentiometer wiper   | **PA0** | `ADC1_IN0` |
| `NTC`  | NTC divider output    | **PA1** | `ADC1_IN1` |
| `LED`  | (driven from MCU)     | **PA5** | GPIO output, mode indicator |
| `NC`   | not connected         | —   | unused pad on the breakout |

The Nucleo board's **onboard B1 button**:

| Signal | MCU pin | Function |
|---|---|---|
| User button (B1) | **PC13** | `EXTI13`, falling edge, mode toggle |


### Peripheral configuration

- **ADC1**: scan conversion mode (fixed sequence: channel 0 → channel 1),
  continuous conversion mode enabled, 12-bit resolution, right-aligned data,
  79.5 ADC clock cycles sampling time on both channels (long enough for the
  NTC voltage divider's source impedance).
- **DMA1 Channel 1**: linked to ADC1, **circular mode**, half-word data width,
  memory increment enabled, peripheral increment disabled.
  `DMAContinuousRequests` is enabled on the ADC so the DMA keeps re-arming
  after every pass through the circular buffer — the buffer is continuously
  refreshed in the background with no CPU intervention per sample.
- **EXTI13** (PC13 / B1): interrupt on falling edge (press pulls the line to
  GND). No internal pull configured (`GPIO_NOPULL`) because the Nucleo-64
  board already provides an external pull-up on B1.
- **GPIO PA5**: push-pull output, used as a mode/status LED.

## Building and flashing

This project uses [PlatformIO](https://platformio.org/).

```sh
# Flash via ST-LINK
pio run -t upload
```

Static analysis (clang-tidy, scoped to `Core/Src/recruiting`):

```sh
pio check -e release
```

## Running tests

_(TBD — no unit tests exist yet. Pure-logic modules (filters, temperature
conversion) will be unit tested natively with Unity, decoupled from the
STM32 HAL so they run on the host without hardware, via `pio test -e tests`.)_

## Operating modes

_(TBD — not implemented yet. Planned design: two mutually exclusive modes,
toggled by pressing the user button (B1):

1. **Streaming mode** (default at boot): the board sends a formatted line of
   sensor data over USART2 every 100 ms, with the currently active filter
   applied.
2. **CLI mode**: streaming stops; the user can type commands over the serial
   terminal to change the active filter. Pressing the button again returns to
   streaming mode.

See [FSM design](#fsm-design) for the full state/transition table once it
exists.)_

## CLI commands

_(TBD — will be documented once the command grammar and parser are
implemented.)_

## FSM design

_(TBD — will be documented once the state machine is implemented. Expected to
cover: state list, entry/exit actions, transition table, and how the button's
EXTI callback interacts with the FSM.)_

## Filters

_(TBD — will be documented once the filter implementations are in place.
Expected to cover: raw passthrough, 150-sample moving average, and
configurable random noise injection.)_

## Serial output format

_(TBD — will be documented once finalized. Draft/example from the task spec:
`POT_RAW:2048,POT_C:24.3,NTC_RAW:1780,NTC_C:31.7\r\n` — note the potentiometer
is not a temperature sensor, so the final format will likely report a
percentage or voltage for POT instead of a Celsius value; see
[Design choices and reasoning](#design-choices-and-reasoning).)_

## Design choices and reasoning

- **ADC + DMA circular mode**: both analog channels (POT, NTC) are sampled
  continuously by the ADC and copied into a RAM buffer by DMA without CPU
  involvement per sample. Circular mode means the DMA automatically wraps and
  keeps refreshing the buffer forever, so `main()` can read the latest values
  at any time (e.g. every 100 ms in streaming mode, or on demand in CLI mode)
  without managing conversion start/stop logic.
- **EXTI for the button** instead of polling: the mode toggle is
  event-driven and doesn't need to be checked on every loop iteration,
  keeping the main loop free for streaming/CLI work. Falling edge + no
  internal pull matches the Nucleo B1 button's existing external pull-up.
- **POT output field**: the task's example output (`POT_C:24.3`) implies a
  Celsius value for the potentiometer, which doesn't correspond to anything
  physical for a position/voltage-divider component. This firmware reports
  `POT_RAW` (raw ADC counts) alongside a derived value appropriate for a
  potentiometer (TBD: percentage or voltage) rather than a fabricated
  temperature, since the task explicitly allows changing the output format.
- _(More entries will be added as filters, the FSM, and the CLI are
  implemented — e.g. why a particular filter buffer strategy was chosen, how
  150-sample moving average interacts with the circular DMA buffer, and how
  mode switching avoids blocking UART I/O.)_

