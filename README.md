# Recruiting Firmware — NTC + Potentiometer Acquisition with CLI

Firmware for the **NUCLEO-C031C6** (STM32C031C6, Cortex-M0+) that samples an NTC
thermistor and a potentiometer via ADC/DMA, applies selectable filters, and
streams the result over the ST-LINK Virtual COM Port (USART2). A CLI mode lets
the user change the active filter at runtime. The user button (B1) toggles
between the two modes.

> **Status: work in progress.** Step 1 of the task is complete; the following
> steps are being implemented. Sections marked `(TBD)` will be filled in as
> the corresponding step lands. See [Project status](#project-status) for
> what is done so far.

## Table of contents

- [Hardware](#hardware)
- [Building and flashing](#building-and-flashing)
- [Running tests](#running-tests)
- [Operating modes](#operating-modes)
- [CLI commands](#cli-commands)
- [FSM design](#fsm-design) (TBD)
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
| `LED`  | (driven from MCU)     | **PA4** | GPIO output for the external LED |
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
- **GPIO PA4**: push-pull output for the external LED
  (`LED_EXT_Pin`/`LED_EXT_GPIO_Port`). Originally assigned to PA5, moved to
  PA4 after discovering the Nucleo-64 Arduino header's silkscreen labels
  (`A0`-`A5`) don't match the STM32's own `PA0`-`PA5` pin names — the
  physical "A5" header pad maps to `PA6`, not `PA5`.
- **USART2**: asynchronous mode, 115200 baud, 8 data bits, no parity, 1 stop
  bit, no hardware flow control, on PA2 (`USART2_TX`) / PA3 (`USART2_RX`) —
  the ST-LINK Virtual COM Port. Output goes through `printf`, routed via
  `__io_putchar` → `HAL_UART_Transmit`. Input is interrupt driven:
  `USART2_IRQn` is enabled and `HAL_UART_Receive_IT` receives one byte at a
  time; `HAL_UART_RxCpltCallback` hands each byte to the CLI and re-arms the
  reception.

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
`test_float_format` and `test_ntc` (`test_cli` is being written).

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

The firmware has two mutually exclusive modes, toggled by pressing the user
button (B1):

1. **Streaming mode** (default once initialization completes): the board
   sends a formatted line of sensor data over USART2 every 100 ms, with the
   currently active filter applied.
2. **CLI mode**: streaming stops and a `> ` prompt appears; the user types
   commands over the serial terminal to change the active filters (see
   [CLI commands](#cli-commands)). Entering CLI mode always starts from an
   empty line. Pressing the button again returns to streaming mode.

During the short window between peripheral initialization and the point
where the firmware selects streaming mode as the startup default, the board
is in a transient startup state and ignores button presses, so an early
press can't leave the mode toggle in an inconsistent state.

See [FSM design](#fsm-design) for the full state/transition table once it
exists.

## CLI commands

Available in CLI mode. A command is a line terminated by Enter (CR or LF);
the task allows changing the grammar, so the commands use short names:

| Command | Effect | Response |
|---|---|---|
| `help`  | Show the list of commands | the command list, plus how filters stack and how to go back to streaming |
| `raw`   | Remove all filters | `removed all the filters applied` |
| `avg`   | Add the 150-sample moving average filter | `moving average filtering enabled` |
| `noise` | Add random noise injection | `noise injection enabled` |

Filters stack: `avg` and `noise` can be active together, and only `raw`
removes them (see [Filters](#filters)). Commands apply to both channels and
take effect on the data sent once streaming resumes; press the user button to
go back to streaming.

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
| `[ERROR] command not found` | The first word is not a known command |
| `[ERROR] empty line` | Enter on a line with no words |
| `[ERROR] too many args` | More than 4 words on the line |

## FSM design

_(TBD — will be documented once the state machine is implemented. Expected to
cover: state list, entry/exit actions, transition table, and how the button's
EXTI callback interacts with the FSM.)_

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

In streaming mode, one line every 100 ms:

```
POT_RAW:<counts>,POT_C:<celsius>,NTC_RAW:<counts>,NTC_C:<celsius>\r\n
```

For example `POT_RAW:2048,POT_C:24.3,NTC_RAW:1780,NTC_C:31.7`.

- `*_RAW` is the 12-bit ADC value **after** the active filters (so it is the
  actual hardware reading only when no filter is active).
- `*_C` is that value converted to degrees Celsius, with one decimal digit.

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
  at any time (e.g. every 100 ms in streaming mode) without managing
  conversion start/stop logic.
- **EXTI for the button** instead of polling: the mode toggle is
  event-driven and doesn't need to be checked on every loop iteration,
  keeping the main loop free for streaming/CLI work. Falling edge + no
  internal pull matches the Nucleo B1 button's existing external pull-up.
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

## Project status

| Step | Description | Status |
|---|---|---|
| 1 | ADC (DMA circular) + EXTI button, filters, streaming and CLI modes | ✅ Done |
| 2 | FSM with `libfsm-sw` (POST, Waiting, Listening, Pause, Warning, Fatal) | ⏳ Not started |
| 3 | Unit tests (PlatformIO native + Unity) | 🟡 In progress: filter, moving average, float formatting and NTC suites done; CLI suite being written |
| 4 | Host-side real-time plot + sound modulation (bonus) | ⏳ Not started |
| 5 | Scheduler with `libtimebase-sw` (bonus) | ⏳ Not started |

