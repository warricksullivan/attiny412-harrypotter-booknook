/*
 * main.c
 *
 * Created: 2/14/2026 3:40:30 PM
 *  Author: Warrick
 *
 * Motion-activated LED timer with 74HC595 shift register:
 * - PA1: SPI MOSI -> 74HC595 SER (pin 14)
 * - PA2 (physical pin 5): Motion sensor input (active low with pull-up)
 * - PA3: SPI SCK -> 74HC595 SRCLK (pin 11)
 * - PA6: Latch pin -> 74HC595 RCLK (pin 12)
 * - LED outputs via shift register (QA-QE for 5 LED strips)
 * - LED turns on immediately when motion detected
 * - LED stays on while motion continues (timer resets continuously)
 * - LED turns off 5 seconds after motion stops
 *
 * Default clock: 20 MHz internal oscillator with /6 prescaler = 3.333 MHz
 */

#define F_CPU 3333333UL  /* 3.333 MHz (20 MHz / 6 prescaler) */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

#define MOTION_PIN   PIN2_bm
#define LATCH_PIN    PIN6_bm
#define TIMEOUT_SEC  5

/* Shift register output bits (5 LED strips on QA-QE) */
#define LED_STRIP_1  (1 << 0)
#define LED_STRIP_2  (1 << 1)
#define LED_STRIP_3  (1 << 2)
#define LED_STRIP_4  (1 << 3)
#define LED_STRIP_5  (1 << 4)
#define ALL_LEDS     (LED_STRIP_1 | LED_STRIP_2 | LED_STRIP_3 | LED_STRIP_4 | LED_STRIP_5)

/* Global timer countdown in seconds (0 = LED off) */
volatile uint8_t led_timer = 0;

/* Current shift register state */
volatile uint8_t shift_reg_state = 0;

/*
 * Initialize SPI in master mode for 74HC595 communication.
 * - PA1 (MOSI): Serial data out
 * - PA3 (SCK): Clock
 * - CLK_PER/64 = ~52 kHz (slower for reliability)
 * - MSB first, mode 0 (CPOL=0, CPHA=0)
 * - SPI is disabled after init; shift_out() enables it per transfer
 */
static void spi_init(void)
{
    /* Configure PA1 (MOSI) and PA3 (SCK) as outputs */
    PORTA.DIRSET = PIN1_bm | PIN3_bm;

    /* Configure SPI master, MSB first, mode 0, CLK/64, but leave disabled */
    SPI0.CTRLA = SPI_MASTER_bm | SPI_PRESC_DIV64_gc;

    /* Mode 0: CPOL=0, CPHA=0, SSD=1 (client select disable) */
    SPI0.CTRLB = SPI_SSD_bm | SPI_MODE_0_gc;
}

/*
 * Shift out a byte to the 74HC595 and latch it.
 * Updates the shift register outputs immediately.
 * Disables SPI after transfer to save power during sleep.
 */
static void shift_out(uint8_t data)
{
    /* Enable SPI before transfer */
    SPI0.CTRLA |= SPI_ENABLE_bm;

    /* Ensure latch is low before shifting */
    PORTA.OUTCLR = LATCH_PIN;

    /* Clear any previous SPI interrupt flag */
    SPI0.INTFLAGS = SPI_IF_bm;

    /* Start SPI transfer */
    SPI0.DATA = data;

    /* Wait for transfer complete */
    while (!(SPI0.INTFLAGS & SPI_IF_bm));

    /* Clear the flag */
    SPI0.INTFLAGS = SPI_IF_bm;

    /* Pulse latch pin HIGH to transfer shift register to output register */
    PORTA.OUTSET = LATCH_PIN;
    PORTA.OUTCLR = LATCH_PIN;

    /* Disable SPI to save power during sleep */
    SPI0.CTRLA &= ~SPI_ENABLE_bm;
}

/*
 * PA2 pin-change ISR — fires on falling edge (motion detected).
 * Turns all LEDs on immediately and resets the timeout.
 */
ISR(PORTA_PORT_vect)
{
    /* Clear the interrupt flag */
    PORTA.INTFLAGS = MOTION_PIN;

    /* Motion detected: turn on all LED strips and reset timer */
    shift_reg_state = ALL_LEDS;
    shift_out(shift_reg_state);
    led_timer = TIMEOUT_SEC;
}

/*
 * TCA0 overflow ISR — ticks at 1 Hz, manages timer countdown.
 * CLK_PER = 3.333 MHz, TCA prescaler /1024 -> 3255 Hz tick rate.
 * PER = 3254 -> overflow at ~1.0 Hz.
 *
 * If PA2 is still active (held low), resets the timer.
 * Otherwise decrements, turning off all LEDs when it reaches 0.
 */
ISR(TCA0_OVF_vect)
{
    /* Clear the interrupt flag */
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

    if (!(PORTA.IN & MOTION_PIN))
    {
        /* Motion sensor still active — keep LEDs on */
        led_timer = TIMEOUT_SEC;
    }
    else if (led_timer > 0)
    {
        led_timer--;

        /* Turn off all LEDs when timer expires */
        if (led_timer == 0)
        {
            shift_reg_state = 0;
            shift_out(shift_reg_state);
        }
    }
}

int main(void)
{
    /* Configure PA6 as output (latch pin for 74HC595) */
    PORTA.DIRSET = LATCH_PIN;
    PORTA.OUTCLR = LATCH_PIN; /* Start low */

    /* Configure PA2 as input with pull-up, falling-edge interrupt */
    PORTA.DIRCLR = MOTION_PIN;
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;

    /* Clear any pending interrupt flag from pin configuration */
    PORTA.INTFLAGS = MOTION_PIN;

    /* Initialize SPI for shift register communication */
    spi_init();

    /* Start with all LEDs off */
    shift_reg_state = 0;
    shift_out(shift_reg_state);

    /* Configure TCA0 in Normal mode:
     * - Prescaler /1024
     * - Period for ~1 second overflow at 3.333 MHz / 1024 = 3255 Hz
     */
    TCA0.SINGLE.PER = 3254;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;

    /* Enable overflow interrupt */
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;

    /* Enable global interrupts */
    sei();

    /* Idle sleep — CPU halts, peripherals and interrupts stay active.
     * Wakes on PA2 pin-change or TCA0 overflow. */
    set_sleep_mode(SLEEP_MODE_IDLE);

    while (1)
    {
        sleep_mode();
    }
}
