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
 * - PA7: Capacitive touch input (copper foil pad behind PLA)
 * - LED outputs via shift register (Q0-Q7 for 8 LED strips)
 * - Per-strip enable/disable via enabled_strips bitmask
 * - Enabled LED strips turn on immediately when motion detected
 * - LED strips stay on while motion continues (timer resets continuously)
 * - LED strips turn off 5 seconds after motion stops
 *
 * Default clock: 20 MHz internal oscillator with /6 prescaler = 3.333 MHz
 */

#define F_CPU 3333333UL  /* 3.333 MHz (20 MHz / 6 prescaler) */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <util/delay.h>

#define MOTION_PIN   PIN2_bm
#define LATCH_PIN    PIN6_bm
#define TIMEOUT_SEC  5

/* Shift register output bits (8 LED strips on QA-QH) */
#define LED_STRIP_1  (1 << 0)
#define LED_STRIP_2  (1 << 1)
#define LED_STRIP_3  (1 << 2)
#define LED_STRIP_4  (1 << 3)
#define LED_STRIP_5  (1 << 4)
#define LED_STRIP_6  (1 << 5)
#define LED_STRIP_7  (1 << 6)
#define LED_STRIP_8  (1 << 7)
#define ALL_LEDS     0xFF

/* Capacitive touch sensing */
#define TOUCH_PIN          PIN7_bm
#define TOUCH_ADC_CH       ADC_MUXPOS_AIN7_gc
#define TOUCH_SAMPLES      64      /* Samples per scan (power of 2) */
#define TOUCH_THRESHOLD    20      /* ADC counts above baseline = touch */
#define TOUCH_DEBOUNCE     5       /* Consecutive readings to confirm */
#define BASELINE_SHIFT     7       /* Baseline adaptation speed (slower) */
#define BASELINE_INIT_CYCLES 16    /* Startup calibration samples */
#define TCB_SCAN_TOP       41666   /* CLK_PER/2 / 41666 ≈ 40 Hz */

/* Global timer countdown in seconds (0 = LED off) */
volatile uint8_t led_timer = 0;

/* Current shift register state */
volatile uint8_t shift_reg_state = 0;

/* Per-strip enable/disable mask.
 * Each bit controls whether that strip participates in motion detection.
 * Strips can still be controlled manually regardless of this mask.
 */
uint8_t motion_enabled_strips = ALL_LEDS;  /* All respond to motion by default */

/* Capacitive touch state */
volatile uint16_t touch_baseline = 0;
volatile uint8_t touch_debounce_cnt = 0;
volatile uint8_t touch_state = 0;  /* 0 = released, 1 = touched */


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
 * Turn on specific LED strip(s) immediately.
 */
static inline void strip_on(uint8_t strip_mask)
{
    shift_reg_state |= strip_mask;
    shift_out(shift_reg_state);
}

/*
 * Turn off specific LED strip(s) immediately.
 */
static inline void strip_off(uint8_t strip_mask)
{
    shift_reg_state &= ~strip_mask;
    shift_out(shift_reg_state);
}

/*
 * Set the strip state directly (turns on only the specified strips).
 */
static inline void strip_set(uint8_t strip_mask)
{
    shift_reg_state = strip_mask;
    shift_out(shift_reg_state);
}

/*
 * Toggle specific LED strip(s).
 */
static inline void strip_toggle(uint8_t strip_mask)
{
    shift_reg_state ^= strip_mask;
    shift_out(shift_reg_state);
}

/*
 * Enable strip(s) to respond to motion detection.
 */
static inline void strip_motion_enable(uint8_t strip_mask)
{
    motion_enabled_strips |= strip_mask;
}

/*
 * Disable strip(s) from responding to motion detection.
 */
static inline void strip_motion_disable(uint8_t strip_mask)
{
    motion_enabled_strips &= ~strip_mask;
}

/*
 * Initialize ADC0 for capacitive touch sensing on PA7.
 * - VDD reference, prescaler /16 (~208 kHz ADC clock)
 * - 10-bit resolution, single conversion mode
 * - PA7 digital input buffer disabled to reduce leakage
 */
static void adc_init(void)
{
    /* Disable digital input buffer on PA7 to reduce noise */
    PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;

    /* CTRLC: VDD ref (REFSEL=0x1 in bits 5:4), prescaler /16 (PRESC=0x3 in bits 2:0)
     * Using explicit hex to rule out define issues with XC8 */
    ADC0.CTRLC = (0x01 << 4) | (0x03 << 0);  /* = 0x13 */

    /* CTRLA: 10-bit (RESSEL=0, bit 2), enable (bit 0) */
    ADC0.CTRLA = (1 << 0);  /* = 0x01 */

    /* Select AIN7 (PA7) */
    ADC0.MUXPOS = 0x07;
}

/*
 * Take a single capacitive touch reading on PA7.
 * Charges the pad by driving HIGH, then floats and samples with ADC.
 * A finger adds capacitance, draining charge faster → lower reading.
 */
static uint16_t touch_measure_once(void)
{
    /* Disconnect ADC from pin during charge to avoid loading the pad */
    ADC0.MUXPOS = 0x1F;  /* GND — disconnect from AIN7 */

    /* Drive PA7 HIGH to charge the pad */
    PORTA.DIRSET = TOUCH_PIN;
    PORTA.OUTSET = TOUCH_PIN;

    /* Longer charge time for reliable charging */
    _delay_us(50);

    /* Switch PA7 to high-Z input (float) */
    PORTA.DIRCLR = TOUCH_PIN;
    PORTA.OUTCLR = TOUCH_PIN;

    /* Reconnect ADC and sample immediately */
    ADC0.MUXPOS = 0x07;  /* AIN7 */

    /* Start ADC conversion */
    ADC0.COMMAND = ADC_STCONV_bm;

    /* Wait for conversion complete */
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));

    /* Reading clears the flag */
    return ADC0.RES;
}

/*
 * Take TOUCH_SAMPLES readings and return the average.
 * Reduces noise for more reliable touch detection.
 */
static uint16_t touch_measure_filtered(void)
{
    uint16_t sum = 0;

    for (uint8_t i = 0; i < TOUCH_SAMPLES; i++)
    {
        sum += touch_measure_once();
    }

    return sum >> 6;  /* Divide by 64 (TOUCH_SAMPLES) */
}

/*
 * Initialize TCB0 for periodic capacitive touch scanning at ~40 Hz.
 * Uses CLK_PER/2 prescaler in periodic interrupt mode.
 */
static void tcb0_init(void)
{
    TCB0.CCMP = TCB_SCAN_TOP;
    TCB0.CTRLA = TCB_CLKSEL_CLKDIV2_gc | TCB_ENABLE_bm;
    TCB0.INTCTRL = TCB_CAPT_bm;
}

/*
 * PA2 pin-change ISR — fires on falling edge (motion detected).
 * Turns on motion-enabled LED strips and resets the timeout.
 */
ISR(PORTA_PORT_vect)
{
    /* Clear the interrupt flag */
    PORTA.INTFLAGS = MOTION_PIN;

    /* Motion detected: turn on motion-enabled LED strips and reset timer */
    shift_reg_state |= motion_enabled_strips;
    shift_out(shift_reg_state);
    led_timer = TIMEOUT_SEC;
}

/*
 * TCA0 overflow ISR — ticks at 1 Hz, manages timer countdown.
 * CLK_PER = 3.333 MHz, TCA prescaler /1024 -> 3255 Hz tick rate.
 * PER = 3254 -> overflow at ~1.0 Hz.
 *
 * If PA2 is still active (held low), resets the timer.
 * Otherwise decrements, turning off motion-enabled LEDs when it reaches 0.
 */
ISR(TCA0_OVF_vect)
{
    /* Clear the interrupt flag */
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

    if (!(PORTA.IN & MOTION_PIN))
    {
        /* Motion sensor still active — keep motion-enabled LEDs on */
        led_timer = TIMEOUT_SEC;
    }
    else if (led_timer > 0)
    {
        led_timer--;

        /* Turn off motion-enabled LEDs when timer expires */
        if (led_timer == 0)
        {
            shift_reg_state &= ~motion_enabled_strips;
            shift_out(shift_reg_state);
        }
    }
}

/*
 * TCB0 capture ISR — fires at ~40 Hz for capacitive touch scanning.
 * Reads filtered ADC value, compares against adaptive baseline,
 * debounces state changes, and toggles all LEDs on touch/release.
 */
ISR(TCB0_INT_vect)
{
    /* Clear the interrupt flag */
    TCB0.INTFLAGS = TCB_CAPT_bm;

    uint16_t reading = touch_measure_filtered();
    uint16_t baseline = touch_baseline;

    /* Touch INCREASES reading (finger holds charge longer on this pad) */
    uint8_t tentative = (reading > baseline) && ((reading - baseline) >= TOUCH_THRESHOLD);

    if (tentative != touch_state)
    {
        touch_debounce_cnt++;

        if (touch_debounce_cnt >= TOUCH_DEBOUNCE)
        {
            touch_state = tentative;
            touch_debounce_cnt = 0;

            if (touch_state)
            {
                strip_on(ALL_LEDS);
            }
            else
            {
                strip_off(ALL_LEDS);
            }
        }
    }
    else
    {
        touch_debounce_cnt = 0;
    }

    /* Adaptive baseline: slowly track readings when not touched */
    if (!touch_state)
    {
        if (reading > baseline)
        {
            touch_baseline += (reading - baseline) >> BASELINE_SHIFT;
        }
        else if (reading < baseline)
        {
            touch_baseline -= (baseline - reading) >> BASELINE_SHIFT;
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

    /* Initialize ADC for capacitive touch sensing */
    adc_init();

    /* Calibrate touch baseline (don't touch pad during power-up!) */
    uint32_t baseline_sum = 0;
    for (uint8_t i = 0; i < BASELINE_INIT_CYCLES; i++)
    {
        baseline_sum += touch_measure_filtered();
    }
    touch_baseline = (uint16_t)(baseline_sum / BASELINE_INIT_CYCLES);

    /* Start capacitive touch scanning at ~40 Hz */
    tcb0_init();

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
