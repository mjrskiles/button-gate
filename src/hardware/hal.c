#include "hardware/hal.h"
#include <avr/eeprom.h>
#include <avr/sleep.h>

static HalInterface default_hal = {
    .max_pin            = HAL_MAX_PIN,
    .button_a_pin       = BUTTON_A_PIN,
    .button_b_pin       = BUTTON_B_PIN,
    .sig_out_pin        = SIG_OUT_PIN,
    .led_mode_top_pin   = LED_MODE_TOP_PIN,
    .led_output_indicator_pin = LED_OUTPUT_INDICATOR_PIN,
    .led_mode_bottom_pin = LED_MODE_BOTTOM_PIN,
    .init               = hal_init,
    .set_pin            = hal_set_pin,
    .clear_pin          = hal_clear_pin,
    .toggle_pin         = hal_toggle_pin,
    .read_pin           = hal_read_pin,
    .init_timer         = hal_init_timer0,
    .millis             = hal_millis,
    .delay_ms           = hal_delay_ms,
    .advance_time       = hal_advance_time,
    .reset_time         = hal_reset_time,
    .eeprom_read_byte   = hal_eeprom_read_byte,
    .eeprom_write_byte  = hal_eeprom_write_byte,
    .eeprom_read_word   = hal_eeprom_read_word,
    .eeprom_write_word  = hal_eeprom_write_word,
    .adc_read           = hal_adc_read,
};

HalInterface *p_hal = &default_hal;

/**
 * Initializes I/O pins for Rev2 hardware.
 *
 * Rev2 Pin Assignments:
 * - PB0: Neopixel data (output, directly driven)
 * - PB1: CV output (output)
 * - PB2: Button A (input, no pull-up - external pull-down)
 * - PB3: CV input (analog input via ADC3, per ADR-004)
 * - PB4: Button B (input, no pull-up - external pull-down)
 * - PB5: RESET (reserved)
 *
 * Also initializes Timer0 for millisecond timing and ADC for CV input.
 */
void hal_init(void) {
    // Configure button pins as inputs
    DDRB &= ~((1 << BUTTON_A_PIN) | (1 << BUTTON_B_PIN));

    // Ensure button pins have no internal pull-up (external pull-down expected)
    PORTB &= ~((1 << BUTTON_A_PIN) | (1 << BUTTON_B_PIN));

    // Set signal out pin as output, initially low
    DDRB |= (1 << SIG_OUT_PIN);
    PORTB &= ~(1 << SIG_OUT_PIN);

    // Set Neopixel pin as output, initially low
    DDRB |= (1 << NEOPIXEL_PIN);
    PORTB &= ~(1 << NEOPIXEL_PIN);

    // CV input pin as input (directly read by ADC, no digital buffer needed)
    DDRB &= ~(1 << CV_IN_PIN);

    // Initialize Timer0 for millisecond timing
    hal_init_timer0();

    // Initialize ADC for CV input (per ADR-004)
    hal_init_adc();
}

/**
 * Sets the specified pin high.
 * 
 * @param pin The pin number to set high
 */
void hal_set_pin(uint8_t pin) {
    PORTB |= (1 << pin);
}

/**
 * Sets the specified pin low.
 * 
 * @param pin The pin number to set low
 */
void hal_clear_pin(uint8_t pin) {
    PORTB &= ~(1 << pin);
}

/**
 * Toggles the specified pin (inverts its current state).
 * 
 * @param pin The pin number to toggle
 */
void hal_toggle_pin(uint8_t pin) {
    PORTB ^= (1 << pin);
}

/**
 * Reads the current state of the specified pin.
 * 
 * @param pin The pin number to read
 * @return 1 if the pin is high, 0 if the pin is low
 */
uint8_t hal_read_pin(uint8_t pin) {
    return (PINB & (1 << pin)) != 0;
}

// =============================================================================
// Timer0 Millisecond Counter
// =============================================================================
//
// ATOMICITY FIX (Klaus audit):
// On 8-bit AVR, incrementing a 32-bit variable is NOT atomic - it compiles to
// multiple load/add/store instructions. If the ISR fires mid-increment during
// a read from main code, corrupted values can result.
//
// Solution: Use 16-bit counter in ISR (atomic on AVR) + extend to 32-bit in
// hal_millis(). The 16-bit counter overflows every ~65 seconds, which we
// detect and handle in the main-thread read function.
//
// =============================================================================

// Low 16 bits: incremented in ISR (atomic on AVR - single instruction)
static volatile uint16_t timer0_millis_low = 0;

// High 16 bits: extended in hal_millis() (only accessed with interrupts disabled)
static uint16_t timer0_millis_high = 0;

// Track previous low value to detect overflow
static uint16_t timer0_millis_last = 0;

// Timer configuration - derived from F_CPU (set in CMakeLists.txt)
#ifndef F_CPU
#error "F_CPU must be defined (e.g., 1000000UL for 1MHz, 8000000UL for 8MHz)"
#endif

#define TIMER0_PRESCALER      8
#define TIMER0_TARGET_HZ      1000  // 1ms intervals
#define TIMER0_COMPARE_VALUE  ((F_CPU / TIMER0_PRESCALER / TIMER0_TARGET_HZ) - 1)

// Compile-time validation
#if TIMER0_COMPARE_VALUE > 255
#error "F_CPU too high for 8-bit timer with prescaler 8. Increase prescaler."
#endif
#if TIMER0_COMPARE_VALUE < 1
#error "F_CPU too low for accurate 1ms timing with prescaler 8."
#endif

/**
 * Initializes Timer0 for millisecond timing.
 * Uses CTC mode with prescaler 8.
 * Compare value is calculated from F_CPU at compile time.
 */
void hal_init_timer0(void) {
    // Set Timer0 to CTC mode
    TCCR0A = (1 << WGM01);

    // Set prescaler to 8 (CS01)
    TCCR0B = (1 << CS01);

    // Set compare value for 1ms intervals
    OCR0A = TIMER0_COMPARE_VALUE;

    // Enable Timer0 compare match interrupt
    TIMSK |= (1 << OCIE0A);

    // Enable global interrupts
    sei();
}

/**
 * Timer0 compare match interrupt handler.
 * Increments 16-bit millisecond counter (atomic on AVR).
 */
ISR(TIMER0_COMPA_vect) {
    timer0_millis_low++;  // Single instruction on AVR - atomic
}

/**
 * Returns the number of milliseconds since the program started.
 *
 * Extends 16-bit ISR counter to 32-bit by detecting overflows.
 * Must be called at least once every 65 seconds to catch overflows.
 * (In practice, main loop runs at 1kHz, so this is never an issue.)
 *
 * @return Number of milliseconds since program start
 */
uint32_t hal_millis(void) {
    uint16_t low;

    // Read low word with interrupts disabled (single 16-bit read is atomic,
    // but we need consistent state for overflow detection)
    cli();
    low = timer0_millis_low;

    // Detect overflow: if low < last, the counter wrapped
    if (low < timer0_millis_last) {
        timer0_millis_high++;
    }
    timer0_millis_last = low;

    sei();

    return ((uint32_t)timer0_millis_high << 16) | low;
}

/**
 * Blocking delay for the specified number of milliseconds.
 *
 * Uses Idle sleep mode for power efficiency. The CPU sleeps between
 * Timer0 interrupts (1ms intervals), waking briefly to check if the
 * delay has elapsed. This reduces power consumption by ~90% compared
 * to busy-waiting.
 *
 * @param ms Number of milliseconds to delay
 */
void hal_delay_ms(uint32_t ms) {
    uint32_t start_time = hal_millis();

    set_sleep_mode(SLEEP_MODE_IDLE);

    while (hal_millis() - start_time < ms) {
        sleep_mode();  // Sleep until Timer0 interrupt (1ms)
    }
}

void hal_advance_time(uint32_t ms) {
    // For testing: directly manipulate the 32-bit time
    cli();
    uint32_t current = ((uint32_t)timer0_millis_high << 16) | timer0_millis_low;
    current += ms;
    timer0_millis_high = (uint16_t)(current >> 16);
    timer0_millis_low = (uint16_t)(current & 0xFFFF);
    timer0_millis_last = timer0_millis_low;
    sei();
}

void hal_reset_time(void) {
    cli();
    timer0_millis_low = 0;
    timer0_millis_high = 0;
    timer0_millis_last = 0;
    sei();
}

/**
 * Reads a byte from EEPROM.
 *
 * @param addr EEPROM address to read from
 * @return The byte value at the specified address
 */
uint8_t hal_eeprom_read_byte(uint16_t addr) {
    return eeprom_read_byte((const uint8_t *)addr);
}

/**
 * Writes a byte to EEPROM.
 * Uses eeprom_update_byte to minimize write cycles (only writes if value differs).
 *
 * @param addr EEPROM address to write to
 * @param value The byte value to write
 */
void hal_eeprom_write_byte(uint16_t addr, uint8_t value) {
    eeprom_update_byte((uint8_t *)addr, value);
}

/**
 * Reads a 16-bit word from EEPROM.
 *
 * @param addr EEPROM address to read from
 * @return The word value at the specified address
 */
uint16_t hal_eeprom_read_word(uint16_t addr) {
    return eeprom_read_word((const uint16_t *)addr);
}

/**
 * Writes a 16-bit word to EEPROM.
 * Uses eeprom_update_word to minimize write cycles (only writes if value differs).
 *
 * @param addr EEPROM address to write to
 * @param value The word value to write
 */
void hal_eeprom_write_word(uint16_t addr, uint16_t value) {
    eeprom_update_word((uint16_t *)addr, value);
}

// =============================================================================
// ADC Functions (per ADR-004)
// =============================================================================

// ADC channel for CV input (PB3 = ADC3)
#define ADC_CHANNEL_CV 3

// ADC prescaler for 8MHz clock -> 1MHz ADC clock (need 50-200kHz for accuracy)
// Prescaler /8 gives 1MHz, which is at the upper limit but acceptable for 8-bit
// Prescaler /64 gives 125kHz, which is more accurate but slower
#define ADC_PRESCALER_64 ((1 << ADPS2) | (1 << ADPS1))

/**
 * Initializes the ADC for CV input reading.
 *
 * Configuration per ADR-004:
 * - Left-adjust result (ADLAR=1) for easy 8-bit reads from ADCH
 * - VCC as reference voltage (0-5V range)
 * - Prescaler /64 for ~125kHz ADC clock at 8MHz system clock
 *
 * Note: Does not start a conversion - call hal_adc_read() for that.
 */
void hal_init_adc(void) {
    // Set reference to VCC, left-adjust result for 8-bit reads
    // MUX bits will be set in hal_adc_read()
    ADMUX = (1 << ADLAR);

    // Enable ADC with prescaler /64 (125kHz ADC clock at 8MHz)
    ADCSRA = (1 << ADEN) | ADC_PRESCALER_64;

    // Disable digital input buffer on ADC3 (PB3) to reduce power consumption
    DIDR0 |= (1 << ADC3D);
}

// ADC timeout: ~200 iterations at 8MHz with /64 prescaler gives ~1.6ms
// Normal conversion takes ~104us, so 1000 iterations is plenty of margin
#define ADC_TIMEOUT_ITERATIONS 1000

// Fallback value if ADC times out (mid-scale)
#define ADC_TIMEOUT_VALUE 128

/**
 * Reads an 8-bit value from the specified ADC channel.
 *
 * Performs a single conversion and returns the result.
 * Blocking call - takes ~13 ADC clock cycles (~104us at 125kHz).
 * Includes timeout to prevent infinite loop on hardware failure.
 *
 * @param channel ADC channel to read (0-3 on ATtiny85)
 * @return 8-bit ADC value (0-255, representing 0-5V), or ADC_TIMEOUT_VALUE on timeout
 */
uint8_t hal_adc_read(uint8_t channel) {
    // Select channel (preserve ADLAR and REFS bits)
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);

    // Start conversion
    ADCSRA |= (1 << ADSC);

    // Wait for conversion to complete with timeout
    uint16_t timeout = ADC_TIMEOUT_ITERATIONS;
    while ((ADCSRA & (1 << ADSC)) && --timeout) {
        // Busy wait with escape hatch
    }

    // Return mid-scale on timeout (fail-safe: CV input reads as ~2.5V)
    if (timeout == 0) {
        return ADC_TIMEOUT_VALUE;
    }

    // Return 8-bit result (left-adjusted, so read ADCH only)
    return ADCH;
}