# Wiring Notes

## Atmel-ICE (SAM port)

| Pin | Signal | Connection         |
|-----|--------|--------------------|
| 2   | GND    | GND                |
| 3   | DATA   | 412 PA0 (pin 6)    |
| 4   | VTG    | V+                 |

## ATtiny412 (SOIC-8)

| Pin | Port | Connection              |
|-----|------|-------------------------|
| 1   | VCC  | V+                      |
| 2   | PA6  | 595 RCLK (pin 12)      |
| 3   | PA7  | Touchpad                |
| 4   | PA1  | 595 SER (pin 14)       |
| 5   | PA2  | Motion Sensor           |
| 6   | PA0  | Atmel-ICE (pin 3)      |
| 7   | PA3  | 595 SRCLK (pin 11)     |
| 8   | GND  | GND                     |

## SN74HC595 (DIP-16)

| Pin | Name  | Connection              |
|-----|-------|-------------------------|
| 1   | QB    | LED2                    |
| 2   | QC    | LED3                    |
| 3   | QD    | LED4                    |
| 4   | QE    | LED5                    |
| 5   | QF    | LED6                    |
| 6   | QG    | LED7                    |
| 7   | QH    | LED8                    |
| 8   | GND   | GND                     |
| 9   | QH'   | NC (daisy-chain out)    |
| 10  | SRCLR | V+ (never reset)        |
| 11  | SRCLK | 412 PA3 (pin 7)        |
| 12  | RCLK  | 412 PA6 (pin 2)        |
| 13  | OE    | GND (always enabled)    |
| 14  | SER   | 412 PA1 (pin 4)        |
| 15  | QA    | LED1                    |
| 16  | VCC   | V+                      |
