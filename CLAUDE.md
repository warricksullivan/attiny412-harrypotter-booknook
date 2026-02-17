# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal embedded C firmware for an **ATtiny412** AVR microcontroller, part of an "HP Book Nook" hardware project. The ATtiny412 is extremely resource-constrained: 4KB flash, 256B SRAM, 128B EEPROM.

## Build System

- **IDE**: Atmel Studio 7.0 (Visual Studio-based, `.atsln` / `.cproj` project files)
- **Compiler**: Microchip XC8 v2.36
- **Device Pack**: ATtiny_DFP 1.10.348
- **Linker Libraries**: libm
- **Build Configurations**:
  - **Debug**: `-Og -g2` (debug optimization + symbols)
  - **Release**: `-Os` with struct packing (size optimization)

Builds are driven through MSBuild via the Atmel Studio IDE. There is no Makefile or CLI build script.

## Source Structure

```
HP Book Nook/
├── HP book Nook.atsln          # Atmel Studio solution file
└── HP Book Nook/
    ├── HP Book Nook.cproj      # MSBuild project config (compiler flags, device settings)
    ├── main.c                  # Application entry point
    └── Debug/                  # Build output
```

## Reference Docs

- ATtiny412 datasheet: `Docs/ATtiny212-214-412-414-416-DataSheet-DS40002287A.pdf`
- 74HC595 shift register datasheet: `Docs/SN74HC595-datasheet.pdf`
- PDFs cannot be read by Claude Code on this machine (missing PDF renderer)
- Code is written from training knowledge of the tinyAVR 0/1-series. Register and bit field names may need verification against the datasheet if compilation fails.
- If a specific peripheral is causing issues, paste the relevant datasheet section as text so Claude can use exact register definitions.

## Current Firmware Architecture

Motion-activated LED strip controller in `main.c`:

### Hardware Connections
- **PA1**: SPI MOSI → 74HC595 SER (pin 14)
- **PA2** (pin 5): Motion sensor input — active-low with internal pull-up, falling-edge pin-change interrupt
- **PA3**: SPI SCK → 74HC595 SRCLK (pin 11)
- **PA6**: Latch GPIO → 74HC595 RCLK (pin 12)
- **PA7**: Capacitive touch input — copper foil pad behind 2mm PLA, charge-and-measure via ADC

### 74HC595 Shift Register
- 8 LED strips connected to Q0–Q7 (pins 15, 1-7)
- Pin 10 (SRCLR) tied to VCC (never reset)
- Pin 13 (OE) tied to GND (always enabled)

### Peripherals
- **SPI0**: Hardware SPI for shift register, MSB-first, mode 0, CLK/64 (~52 kHz)
  - Disabled between transfers to save power (~50-100µA reduction)
- **TCA0**: 1 Hz timer (3.333 MHz / 1024 prescaler, PER=3254)
- **TCB0**: 40 Hz periodic interrupt for capacitive touch scanning (CLK_PER/2, CCMP=41666)
- **ADC0**: 10-bit, VDD reference, /16 prescaler (~208 kHz), single conversion for touch sensing
- **Sleep**: IDLE mode between interrupts (TCB0, TCA0, ADC all run in IDLE)

### Firmware Flow
1. PA2 falling edge ISR turns on motion-enabled LED strips and resets 5-second countdown
2. TCA0 1 Hz ISR checks if PA2 still held low (resets timer if so), otherwise decrements
3. When timer reaches 0, turns off motion-enabled strips (manually-controlled strips remain on)
4. TCB0 40 Hz ISR scans capacitive touch: touch turns on all LEDs, release turns them off
5. Main loop calls `sleep_mode()`

### Capacitive Touch Sensing
- **Technique**: Charge-and-measure — drive PA7 HIGH (charge pad), float, sample with ADC
- **ADC mux disconnected during charge** to prevent loading the pad (MUXPOS set to GND, then switched to AIN7 before sampling)
- **Finger increases reading** (holds charge longer) — detection is reading > baseline, not below
- 64-sample averaging per scan for noise rejection
- Adaptive baseline with slow tracking (shift 7) when untouched
- Debounce: 5 consecutive readings above threshold (20 ADC counts) to confirm state change
- Baseline calibrated from 16 filtered readings at startup (don't touch pad during power-up)
- Key functions: `adc_init()`, `touch_measure_once()`, `touch_measure_filtered()`, `tcb0_init()`
- Key constants: `TOUCH_PIN`, `TOUCH_THRESHOLD`, `TOUCH_SAMPLES`, `TOUCH_DEBOUNCE`, `BASELINE_SHIFT`

### API Functions
- **Direct control**: `strip_on()`, `strip_off()`, `strip_set()`, `strip_toggle()`
- **Motion control**: `strip_motion_enable()`, `strip_motion_disable()`
- Strips can be controlled manually independent of motion detection

Key constants: `MOTION_PIN`, `LATCH_PIN`, `TIMEOUT_SEC`, `TOUCH_PIN`, `TOUCH_THRESHOLD`, `TOUCH_SAMPLES`, `TOUCH_DEBOUNCE`, `BASELINE_SHIFT`, `LED_STRIP_1` through `LED_STRIP_8`, `ALL_LEDS`.

## Planned Features

- **Touch gestures** — long press, tap patterns for mode control (touch detection working, gestures not yet implemented)
- **Touch vs motion conflict** — touch release currently turns off all LEDs including motion-activated ones; needs proper state management
- **EEPROM storage** — persist strip enable/disable state across power cycles
- **Configuration mode** — button sequence to enter config mode for setting which strips respond to motion

## Development Notes

- All code must target the XC8 compiler and use `<avr/io.h>` as the hardware abstraction header (not `<xc.h>`)
- Respect the ATtiny412's tight constraints — avoid dynamic allocation, keep stack usage minimal, prefer fixed-size buffers
- Use `#define` constants for pin assignments and timing values — no magic numbers
- Prefer interrupt-driven design with idle sleep over busy-polling
- ADC peripheral quirks:
  - XC8 `_gc` defines for ADC registers may not match expected values — use explicit hex (e.g., `ADC0.CTRLC = 0x13` not `ADC_REFSEL_VDDREF_gc | ADC_PRESC_DIV16_gc`)
  - MUXPOS for GND is 0x1F (not 0x1D) on ATtiny412
  - Disconnect ADC mux from pin during charge phase to avoid loading capacitive touch pad
- SPI peripheral quirks:
  - CTRLB must be set with `SPI_SSD_bm | SPI_MODE_0_gc` (setting to 0 doesn't work)
  - Always clear SPI interrupt flag before and after transfer
  - Disable SPI between transfers for power savings
