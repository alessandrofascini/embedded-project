# Recruiting Firmware — NTC + Potentiometer Acquisition with CLI

Firmware for the **NUCLEO-C031C6** (STM32C031C6, Cortex-M0+) that samples an NTC
thermistor and a potentiometer via ADC/DMA, applies selectable filters, and
streams the result over the ST-LINK Virtual COM Port (USART2). A state machine
built on `libfsm-sw` runs a power-on self test, then moves between waiting,
listening, paused, warning and fatal states, driven by the user button (B1),
a CLI command and the sensor readings. Two LEDs show the current state through
timer PWM.

> **Status: work in progress.** Steps 1 and 2 of the task are complete; the
> unit tests of the state machine are being written. See
> [Project status](#project-status) for what is done so far.

## Table of contents

- [Hardware](#hardware)
- [Building and flashing](#building-and-flashing)
- [Running tests](#running-tests)
- [Operating modes](#operating-modes)
- [CLI commands](#cli-commands)
- [FSM design](#fsm-design)
- [Filters](#filters)
- [Serial output format](#serial-output-format)
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
| `LED`  | (driven from MCU)     | **PA4** | `TIM14_CH1` PWM, external LED |
| `NC`   | not connected         | —   | unused pad on the breakout |

The Nucleo board's onboard parts:

| Signal | MCU pin | Function |
|---|---|---|
| User button (B1) | **PC13** | `EXTI13`, falling edge: click trigger; a 2 s hold is detected by polling |
| User LED (LD4) | **PA5** | `TIM1_CH1` PWM, board LED |


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
- **TIM1 channel 1** (PA5, board LED): PWM mode 1. Blinking uses a 1 kHz
  timer tick, so both the period (auto-reload) and the on time (compare)
  are counted in milliseconds.
- **TIM14 channel 1** (PA4, external LED): PWM mode 1. Breathing and fixed
  brightness use a 1 kHz PWM carrier (1 MHz tick, 1000 ticks per period);
  for breathing, the update interrupt fires once per carrier period (1 ms)
  and moves the duty along a triangle wave. The external LED was originally
  on PA5, and moved to PA4 after discovering the Nucleo-64 Arduino header's
  silkscreen labels (`A0`-`A5`) don't match the STM32's own `PA0`-`PA5` pin
  names — the physical "A5" header pad maps to `PA6`, not `PA5`.
- **USART2**: asynchronous mode, 115200 baud, 8 data bits, no parity, 1 stop
  bit, no hardware flow control, on PA2 (`USART2_TX`) / PA3 (`USART2_RX`) —
  the ST-LINK Virtual COM Port. Output goes through `serial_print`, a small
  printf-like formatter that writes each character with `HAL_UART_Transmit`.
  Input is interrupt driven: `USART2_IRQn` is enabled and
  `HAL_UART_Receive_IT` receives one byte at a time; `HAL_UART_RxCpltCallback`
  re-arms the reception, then hands the byte to the CLI only while the state
  machine has the CLI enabled.

## Building and flashing

This project uses [PlatformIO](https://platformio.org/).

```sh
# Flash via ST-LINK
pio run -t upload
```

```sh
# Open the serial monitor (ST-LINK Virtual COM Port, 115200 baud)
pio device monitor -b 115200
```

Static analysis (clang-tidy, scoped to `Core/Src/recruiting`):

```sh
pio check -e release
```

## Running tests

The hardware-agnostic modules under `Core/Src/recruiting/` are unit tested
natively with [Unity](http://www.throwtheswitch.org/unity), decoupled from the
STM32 HAL so they run on the host without hardware. Each module has its own
suite under `test/`: `test_adc_filter`, `test_adc_moving_average`,
`test_float_format`, `test_ntc`, `test_cli` and `test_system_fsm`. Where a
module calls out to code supplied by the caller (the CLI command handlers,
the hardware callbacks of the state machine), the tests replace it with
fakes generated by [FFF](https://github.com/meekrosoft/fff).

```sh
pio test -e tests
```

Run a single test suite (by its folder name under `test/`):

```sh
pio test -e tests -f test_float_format
```

> **Note:** the native `tests` environment requires a host compiler that
> supports `-std=gnu23` (**GCC 14+**). On Ubuntu 22.04 the default `gcc` is
> too old (11.x; even `gcc-12`/`gcc-13` only support the pre-standardization
> `-std=gnu2x` alias). If `pio test` fails with
> `unrecognized command-line option '-std=gnu23'`, install GCC 14:
> ```sh
> sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
> sudo apt-get update
> sudo apt-get install -y gcc-14 g++-14
> ```
> PlatformIO's native platform always resolves the generic `gcc`/`g++` names
> from `PATH` (it has no `CC`/`CXX` override hook), so create a
> `.toolchain-tests/` directory in the repo root (it is git-ignored) holding
> `gcc`/`g++` symlinks pointing at `gcc-14`/`g++-14`:
> ```sh
> mkdir -p .toolchain-tests
> ln -sf "$(command -v gcc-14)" .toolchain-tests/gcc
> ln -sf "$(command -v g++-14)" .toolchain-tests/g++
> ```
> Then prepend it to `PATH` before running tests:
> ```sh
> PATH="$(pwd)/.toolchain-tests:$PATH" pio test -e tests
> ```
> (or add that directory to your shell's `PATH` permanently, e.g. in
> `~/.bashrc`, so plain `pio test` picks it up automatically.)

## Operating modes

The firmware alternates between two kinds of state:

1. **CLI states** (Waiting, Paused): a `> ` prompt is shown and the user types
   commands over the serial terminal (see [CLI commands](#cli-commands)).
   Sensor reading is off. Entering a CLI state always starts from an empty
   line.
2. **Streaming states** (Listening, Warning): the CLI is off and the board
   sends a line of sensor data every 100 ms, with the active filters applied.

A button click or the `run` command starts streaming; a click while
listening pauses it. See [FSM design](#fsm-design) for the full list of
states and transitions.

## CLI commands

Available in the Waiting and Paused states. A command is a line terminated by
Enter (CR or LF); the task allows changing the grammar, so the commands use
short names:

| Command | Effect | Response |
|---|---|---|
| `help`  | Show the list of commands | the command list, plus how filters stack |
| `raw`   | Remove all filters | `removed all the filters applied` |
| `avg`   | Add the 150-sample moving average filter | `moving average filtering enabled` |
| `noise` | Add random noise injection | `noise injection enabled` |
| `run`   | Start streaming (go to Listening) | none: the sensor lines start |

Filters stack: `avg` and `noise` can be active together, and only `raw`
removes them (see [Filters](#filters)). Commands apply to both channels and
take effect on the data sent once streaming resumes; `run` or a button click
goes back to streaming.

Line input:

- Typed characters are echoed by reprinting the prompt and the current line.
- Backspace (`\b`) deletes the last character.
- A line holds up to 63 characters; further characters, Enter included, are
  ignored until one is deleted.
- After Enter, characters typed before the next `> ` prompt are ignored: the
  line cannot change while it is waiting for, or under, execution.
- Tokens are separated by spaces or tabs; a line holds at most 4 tokens.
- Command names must match exactly (`av` or `avgx` are not `avg`).

Errors:

| Message | Cause |
|---|---|
| `command not found` | The first word is not a known command |
| `empty line` | Enter on a line with no words |
| `too many args` | More than 4 words on the line |

## FSM design

The state machine lives in `Core/Src/recruiting/system-fsm/` and is built on
`libfsm-sw` (dev branch). It never touches the hardware: `main.c` hands it
callbacks for the self-test, the raw ADC values, the LEDs, the CLI, serial
output, the tick and the MCU reset.

### States

| State | CLI | Board LED (LD4) | External LED | Sensors | Serial output |
|---|---|---|---|---|---|
| **POST** | off | not driven | not driven | self-test only | — |
| **Waiting** | on | blink, 500 ms, 15 % | breathing, 2 s | off | CLI prompt |
| **Listening** | off | blink, 200 ms, 80 % | duty from NTC − POT (below) | every 100 ms | sensor line every 100 ms |
| **Paused** | on | blink, 2000 ms, 50 % | breathing, 2 s | off | CLI prompt |
| **Warning** | off | off | off | every 100 ms | sensor line every 100 ms, `WARNING` every 200 ms |
| **Fatal** | off | blink, 40 ms, 50 % | off | off | `[ERROR] ...` every 100 ms |

In Listening the external LED duty is `(NTC − POT) / 5 °C`, clamped to
0–100 %: off when the NTC is not warmer than the potentiometer, fully on at
a 5 °C difference or more.

### Transitions

| From | Event | To |
|---|---|---|
| POST | self-test passes | Waiting |
| POST | self-test fails | Fatal |
| Waiting | button click or `run` | Listening |
| Listening | button click | Paused |
| Listening | NTC − POT above 5 °C at every sample for 5 s | Warning |
| Paused | button click or `run` | Listening |
| Warning | button held for 2 s | Waiting |
| Warning | 10 s in the state | Fatal |
| Fatal | button click | MCU reset (`NVIC_SystemReset`) |

Any other event is ignored in that state: for example a click in Warning
does nothing, only a 2 s hold leaves it. One sample at or below the
threshold restarts the 5 s window, and leaving Listening (e.g. through
Paused) does too.

### Events

- **Button click**: the EXTI falling-edge callback posts it at the start of
  every press, long ones included.
- **Button hold**: the main loop polls the button pin and posts it once per
  press, when the press reaches 2 s.
- **`run`**: posted by the CLI command handler, in the main loop.

A state looks up each event in a state × event table. The handler only
*requests* the transition: `system_fsm_run()`, called from the main loop,
runs the state routine, then runs the enter function of the requested
state and switches to it. Enter functions set the LED patterns, open or
close the CLI and restart the state's timers. The one exception is the click
in Fatal, which resets the MCU at once.

Known limitation: the click is posted from the interrupt, and shares the
pending request with the state routines. If both request a transition in
the same step (a click in Listening while the 5 s window expires), the last
one written wins. The planned fix is to queue the event in the interrupt and
handle it at the start of `system_fsm_run()`.

### Self-test and fatal errors

The POST state starts the UART reception, both LED timers and the ADC DMA,
waits up to 100 ms for the first conversion, and checks that the NTC reading
is within 50–4045 (outside it, the sensor is open or shorted). The first
failure becomes the fatal error, printed every 100 ms as
`[ERROR] <message> (code: <n>)`:

| Code | Message |
|---|---|
| 2 | `failed to start the serial reception` |
| 3 | `failed to start the sensors` |
| 4 | `no sensor conversion within the timeout` |
| 5 | `NTC reading at a rail: open or shorted` |
| 6 | `failed to start the LED timers` |
| 7 | `warning state persisted for more than 10 s` |

Only the first error is kept: it is the root cause.

### Initialization errors

`system_fsm_init()` checks every callback and sets up the sensor filters and
models from the board configuration. If it fails, `main.c` prints
`[FATAL] system_fsm_init failed: <code>` and halts. A failure inside a
sub-module is reported as the base of a range plus that module's own error
code, so a single number says both where and why:

| Range | Failing part | Sub-code |
|---|---|---|
| 100 + n | potentiometer filter | `enum AdcFilterError` |
| 200 + n | NTC filter | `enum AdcFilterError` |
| 300 + n | potentiometer model | `enum NtcError` |
| 400 + n | NTC model | `enum NtcError` |

For example `305` is the potentiometer model rejecting its beta
(`NTC_INVALID_BETA` = 5).

## Filters

Each channel (POT, NTC) has its own `AdcFilter`, applied to the latest ADC
sample once per streamed line:

- **Raw**: the sample is passed through unchanged.
- **Noise**: adds a pseudo-random offset in `[-20, +20]` ADC counts, clamped
  to the 12-bit range `[0, 4095]`. The offset comes from a 16-bit Fibonacci
  LFSR (polynomial x¹⁶ + x¹⁴ + x¹³ + x¹¹ + 1, maximal period 65535).
- **Moving average**: the mean of the last 150 samples, kept with a running
  sum so each sample costs O(1). While the window is still filling up, the
  output is the mean of the samples received so far. Since the filter is fed
  once per streamed line (every 100 ms), the window spans about 15 s of
  streaming.

When both are active, noise is injected **before** averaging, so the
average smooths the noise out.

Filters are cumulative and driven by events, one per CLI command
(`raw` → CLEAR, `noise` → ADD_NOISE, `avg` → ADD_AVERAGE):

| State \ Event | CLEAR | ADD_NOISE | ADD_AVERAGE |
|---|---|---|---|
| RAW | RAW | NOISE | MOVING_AVERAGE |
| NOISE | RAW | NOISE | NOISE_AND_AVERAGE |
| MOVING_AVERAGE | RAW | NOISE_AND_AVERAGE | MOVING_AVERAGE |
| NOISE_AND_AVERAGE | RAW | NOISE_AND_AVERAGE | NOISE_AND_AVERAGE |

CLEAR also empties the moving-average window, so averaging always starts
from scratch when it is enabled, never mixing in samples from a previous
activation. Re-sending an event for a filter that is already active changes
nothing.

## Serial output format

In Listening and Warning, one line every 100 ms:

```
POT_RAW:<counts>,POT_C:<celsius>,NTC_RAW:<counts>,NTC_C:<celsius>\r\n
```

For example `POT_RAW:2048,POT_C:24.3,NTC_RAW:1780,NTC_C:31.7`.

- `*_RAW` is the 12-bit ADC value **after** the active filters (so it is the
  actual hardware reading only when no filter is active).
- `*_C` is that value converted to degrees Celsius, with one decimal digit.

Other lines:

| Line | When |
|---|---|
| `WARNING` | every 200 ms in Warning, between the sensor lines |
| `[ERROR] <message> (code: <n>)` | every 100 ms in Fatal (see [FSM design](#fsm-design)) |
| `[FATAL] system_fsm_init failed: <code>` | once, if the state machine cannot start |

The conversion uses the Beta model of a thermistor. The resistance is
derived from the voltage divider, then turned into a temperature:

```
R     = R_fixed · (4095 / raw − 1)
T[K]  = 1 / (1/T0 + ln(R / R0) / β)
T[°C] = T[K] − 273.15
```

| Channel | R_fixed | R0 | T0 | β |
|---|---|---|---|---|
| NTC | 18 kΩ | 8755 Ω | 25 °C | 3950 |
| POT | 5 kΩ | 5 kΩ | 25 °C | 3950 |

## Design choices and reasoning

- **ADC + DMA circular mode**: both analog channels (POT, NTC) are sampled
  continuously by the ADC and copied into a RAM buffer by DMA without CPU
  involvement per sample. Circular mode means the DMA automatically wraps and
  keeps refreshing the buffer forever, so `main()` can read the latest values
  at any time (e.g. every 100 ms in Listening) without managing
  conversion start/stop logic.
- **EXTI for the click, polling for the hold**: a click is a single edge, so
  the falling-edge interrupt catches it without checking the pin on every
  loop iteration. A 2 s hold is a duration, not an edge: the main loop
  compares the pin with the tick and posts the hold once per press. Falling
  edge + no internal pull matches the Nucleo B1 button's existing external
  pull-up.
- **POT reported as a temperature**: the potentiometer is not a temperature
  sensor, but it is converted to °C with the same Beta model as the NTC
  (with its own constants), as in the task's example output (`POT_C:24.3`).
  This makes it an adjustable reference temperature: in Step 2, the Listening
  state compares "thermistor − potentiometer" against a threshold, which
  only makes sense if both values are in the same unit.
- **CLI split into three parts**: a `CommandBuffer` collects the bytes of the
  line being typed, a `CommandExecutor` tokenizes a complete line and calls
  the matching handler from a command table, and the `Cli` facade glues them
  together and tracks the line state. None of them performs I/O: receiving
  bytes and printing are left to `main.c`. This keeps the modules
  hardware-agnostic and testable on the host, and the command table makes
  adding a command a one-line change.
- **Bytes in the interrupt, commands in the main loop**: the UART RX interrupt
  only appends the received byte to the line, which is short and bounded.
  Executing a command (parsing, calling the handler, printing) happens in the
  main loop, so slow work never runs in interrupt context.
- **Filters as an event-driven state machine**: the four filter combinations
  are the states, and each CLI command is an event. Keeping events separate
  from states means no command can jump to an invalid combination, and the
  complete behavior fits in one transition table (see [Filters](#filters)).
  The active state is stored as a function pointer, so applying the filter
  is a single indirect call with no branching on the mode.
- **LFSR for the noise**: a 16-bit LFSR is a few shifts and XORs, needs no
  library, and is deterministic, so the noise sequence is reproducible in
  unit tests.
- **Hardware-agnostic state machine**: the FSM gets every hardware action
  (self-test, raw ADC values, LEDs, CLI, serial output, tick, reset) as a
  callback from `main.c`. The same code runs on the board and in the host
  tests, where each callback, the tick included, is replaced by a fake.
- **Tables instead of branches**: what each event does in each state is a
  state × event table of handlers (an empty entry means "ignored"), and the
  LED patterns are a state × LED table. Changing a behavior is a one-entry
  change, and the whole behavior can be read in one place.
- **The FSM owns the sensor pipeline**: the two filters and the two Beta
  models live in the FSM and are set up by `system_fsm_init()` from a board
  configuration. `main.c` only copies the raw values out of the DMA buffer,
  so the filtering and conversion are covered by the host tests too.
- **Nested error codes**: an initialization failure inside a sub-module is
  returned as a range base plus the sub-module's own code (e.g. `305`), so a
  single return value says which part failed and why, without a decoding
  table on the device.
- **`serial_print` instead of `printf`**: newlib's stdio took about 4 KB of
  the 32 KB flash, leaving about 200 bytes free. A small formatter covering
  only the conversions the firmware uses, plus a custom `__assert_func` (the
  default one pulls stdio in as well), brought flash usage from 99.4 % to
  86.3 %.

## Project status

| Step | Description | Status |
|---|---|---|
| 1 | ADC (DMA circular) + EXTI button, filters, streaming and CLI modes | ✅ Done |
| 2 | FSM with `libfsm-sw` (POST, Waiting, Listening, Pause, Warning, Fatal) | ✅ Done |
| 3 | Unit tests (PlatformIO native + Unity) | 🔄 Done for filter, moving average, float formatting, NTC and CLI; state machine in progress (initialization covered) |
| 4 | Host-side real-time plot + sound modulation (bonus) | ⏳ Not started |
| 5 | Scheduler with `libtimebase-sw` (bonus) | ⏳ Not started |

