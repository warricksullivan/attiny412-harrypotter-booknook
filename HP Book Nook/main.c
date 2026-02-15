/*
 * main.c
 *
 * Created: 2/14/2026 3:40:30 PM
 *  Author: Warrick
 *
 * Motion-activated LED timer:
 * - PA2 (physical pin 5): Motion sensor input (active low with pull-up)
 * - PA6: LED output
 * - LED turns on immediately when motion detected
 * - LED stays on while motion continues (timer resets continuously)
 * - LED turns off 5 seconds after motion stops
 *
 * Default clock: 20 MHz internal oscillator with /6 prescaler = 3.333 MHz
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define LED_PIN      PIN6_bm
#define MOTION_PIN   PIN2_bm
#define TIMEOUT_SEC  5

/* Global timer countdown in seconds (0 = LED off) */
volatile uint8_t led_timer = 0;

/*
 * PA2 pin-change ISR — fires on falling edge (motion detected).
 * Turns LED on immediately and resets the timeout.
 */
ISR(PORTA_PORT_vect)
{
    /* Clear the interrupt flag */
    PORTA.INTFLAGS = MOTION_PIN;

    /* Motion detected: turn on LED and reset timer */
    PORTA.OUTSET = LED_PIN;
    led_timer = TIMEOUT_SEC;
}

/*
 * TCA0 overflow ISR — ticks at 1 Hz, manages timer countdown.
 * CLK_PER = 3.333 MHz, TCA prescaler /1024 -> 3255 Hz tick rate.
 * PER = 3254 -> overflow at ~1.0 Hz.
 *
 * If PA2 is still active (held low), resets the timer.
 * Otherwise decrements, turning off the LED when it reaches 0.
 */
ISR(TCA0_OVF_vect)
{
    /* Clear the interrupt flag */
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

    if (!(PORTA.IN & MOTION_PIN))
    {
        /* Motion sensor still active — keep LED on */
        led_timer = TIMEOUT_SEC;
    }
    else if (led_timer > 0)
    {
        led_timer--;

        /* Turn off LED when timer expires */
        if (led_timer == 0)
        {
            PORTA.OUTCLR = LED_PIN;
        }
    }
}

int main(void)
{
    /* Configure PA6 as output (LED) */
    PORTA.DIRSET = LED_PIN;

    /* Configure PA2 as input with pull-up, falling-edge interrupt */
    PORTA.DIRCLR = MOTION_PIN;
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;

    /* Start with LED off */
    PORTA.OUTCLR = LED_PIN;

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
