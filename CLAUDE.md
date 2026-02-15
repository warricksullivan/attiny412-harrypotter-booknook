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

## Development Notes

- All code must target the XC8 compiler and use `<xc.h>` as the hardware abstraction header.
- Respect the ATtiny412's tight constraints — avoid dynamic allocation, keep stack usage minimal, prefer fixed-size buffers.
- The project is in early/skeleton stage with a single `main.c` containing the main loop.
