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

- The ATtiny412 datasheet is at `Docs/ATtiny212-214-412-414-416-DataSheet-DS40002287A.pdf` but cannot be read by Claude Code on this machine (missing PDF renderer).
- Code is written from training knowledge of the tinyAVR 0/1-series. Register and bit field names may need verification against the datasheet if compilation fails.
- If a specific peripheral is causing issues, paste the relevant datasheet section as text so Claude can use exact register definitions.

## Current Firmware Architecture

Motion-activated LED timer in `main.c`:

- **PA2** (pin 5): Motion sensor input — active-low with internal pull-up, falling-edge pin-change interrupt
- **PA6**: LED output
- **TCA0**: 1 Hz timer (3.333 MHz / 1024 prescaler, PER=3254)
- **Sleep**: Idle mode between interrupts (~0.8 mA vs ~3 mA polling)

Flow: PA2 falling edge ISR turns LED on and resets a 5-second countdown. TCA0 1 Hz ISR checks if PA2 is still held low (resets timer if so), otherwise decrements. LED turns off when timer reaches 0. Main loop just calls `sleep_mode()`.

Key constants defined at top of `main.c`: `LED_PIN`, `MOTION_PIN`, `TIMEOUT_SEC`.

## Planned Features

The ATtiny412 has only 5 usable GPIOs (PA1–PA3, PA6–PA7). To support the full feature set, a **74HC595 shift register** will expand outputs via SPI.

- **Capacitive on/off button** — master power toggle
- **5 LED strip outputs** — driven via 74HC595 shift register (SPI on PA1/PA3)
- **Motion sensor** — existing PA2 input, triggers all enabled strips
- **Per-strip enable/disable** — stored in shift register output state

## Development Notes

- All code must target the XC8 compiler and use `<xc.h>` as the hardware abstraction header.
- Respect the ATtiny412's tight constraints — avoid dynamic allocation, keep stack usage minimal, prefer fixed-size buffers.
- Use `#define` constants for pin assignments and timing values — no magic numbers.
- Prefer interrupt-driven design with idle sleep over busy-polling.
