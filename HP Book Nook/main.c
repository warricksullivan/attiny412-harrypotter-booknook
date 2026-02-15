/*
 * main.c
 *
 * Created: 2/14/2026 3:40:30 PM
 *  Author: Warrick
 *
 * Blinks an LED on PA6 at 1 Hz (toggle every 1 second).
 * Uses TCA0 in Normal mode with overflow interrupt.
 *
 * Default clock: 20 MHz internal oscillator with /6 prescaler = 3.333 MHz
 */

#include <avr/io.h>
#include <avr/interrupt.h>

/*
 * TCA0 overflow ISR — toggles the LED.
 * CLK_PER = 3.333 MHz, TCA prescaler /1024 -> 3255 Hz tick rate.
 * PER = 3254 -> overflow at ~1.0 Hz.
 */

ISR(TCA0_OVF_vect)
{
    /* Clear the interrupt flag */
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

    /* Toggle LED on PA6 */
    PORTA.OUTTGL = PIN6_bm;
}

int main(void)
{
    /* Configure PA6 as output */
    PORTA.DIRSET = PIN6_bm;

    /* Start with LED off */
    PORTA.OUTCLR = PIN6_bm;

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

    while (1)
    {
        /* LED toggling handled by TCA0 overflow ISR */
    }
}
