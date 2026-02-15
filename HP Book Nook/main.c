/*
 * main.c
 *
 * Created: 2/14/2026 3:40:30 PM
 *  Author: Warrick
 *
 * Button-controlled LED timer:
 * - PA2 (physical pin 5): Button input (active high)
 * - PA6: LED output
 * - When button pressed, LED turns on for 20 seconds
 * - Each button press resets the 20-second timer
 *
 * Default clock: 20 MHz internal oscillator with /6 prescaler = 3.333 MHz
 */

#include <avr/io.h>
#include <avr/interrupt.h>

/* Global timer countdown in seconds (0 = LED off) */
volatile uint8_t led_timer = 0;

/*
 * TCA0 overflow ISR — ticks at 1 Hz, decrements timer.
 * CLK_PER = 3.333 MHz, TCA prescaler /1024 -> 3255 Hz tick rate.
 * PER = 3254 -> overflow at ~1.0 Hz.
 */
ISR(TCA0_OVF_vect)
{
    /* Clear the interrupt flag */
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

    /* Decrement timer if active */
    if (led_timer > 0)
    {
        led_timer--;

        /* Turn off LED when timer expires */
        if (led_timer == 0)
        {
            PORTA.OUTCLR = PIN6_bm;
        }
    }
}

int main(void)
{
    /* Configure PA6 as output (LED) */
    PORTA.DIRSET = PIN6_bm;

    /* Configure PA2 as input (button) */
    PORTA.DIRCLR = PIN2_bm;

    /* Enable pull-up on PA2 (button connects to GND when pressed) */
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm;

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

    uint8_t last_button_state = 0;

    while (1)
    {
        /* Read button state (active low with pull-up) */
        uint8_t button_pressed = !(PORTA.IN & PIN2_bm);

        /* Detect button press (transition from not pressed to pressed) */
        if (button_pressed && !last_button_state)
        {
            /* Reset timer to 20 seconds */
            led_timer = 20;

            /* Turn on LED */
            PORTA.OUTSET = PIN6_bm;
        }

        last_button_state = button_pressed;
    }
}
