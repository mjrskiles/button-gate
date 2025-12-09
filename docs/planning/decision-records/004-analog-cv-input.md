# ADR-004: Analog CV Input with Software Hysteresis

## Status

Proposed

## Date

2025-12-08

## Context

Gatekeeper's CV input (PB3) accepts external gate, trigger, or clock signals from other Eurorack modules. These signals are nominally 0-5V but in practice can be noisy, slow-rising, or hover near threshold voltages.

The original design used PB3 as a digital input, relying on the ATtiny85's internal Schmitt trigger (with ~0.3V hysteresis). This works for clean signals but has limitations:

1. **Fixed threshold**: The internal threshold (~1.4V for low-to-high, ~1.1V for high-to-low) cannot be adjusted
2. **Narrow hysteresis**: 0.3V may not be enough for noisy signals
3. **No software control**: Cannot implement debouncing or filtering
4. **Limited diagnostics**: Cannot observe actual voltage level for debugging

Many Eurorack signals benefit from configurable thresholds:
- Gate signals may need higher thresholds to reject noise
- Slow-attack envelopes need wider hysteresis to avoid chatter
- Different modules output different voltage ranges (some peak at 5V, others at 8V+)

## Decision

### Use ADC for CV Input

Read CV input via the ATtiny85's 10-bit ADC (configured for 8-bit mode to save processing):

```c
// ADC configuration for PB3 (ADC3)
ADMUX = (1 << ADLAR) | (1 << MUX1) | (1 << MUX0);  // Left-adjust, ADC3
ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0); // Enable, prescaler /8
```

8-bit resolution (0-255) maps to 0-5V input, giving ~20mV per step - adequate for gate/trigger detection.

Note: Per ADR-001, the system runs at 8MHz (not 1MHz), which provides ample headroom for ADC operations.

### Software Hysteresis

Implement Schmitt trigger behavior in software with configurable thresholds:

```c
typedef struct {
    uint8_t high_threshold;  // ADC value to transition low->high (default: 128 = 2.5V)
    uint8_t low_threshold;   // ADC value to transition high->low (default: 77 = 1.5V)
    bool current_state;      // Debounced digital state
} CVInput;

bool cv_input_update(CVInput *cv, uint8_t adc_value) {
    if (cv->current_state) {
        // Currently HIGH - need to drop below low_threshold to go LOW
        if (adc_value < cv->low_threshold) {
            cv->current_state = false;
        }
    } else {
        // Currently LOW - need to rise above high_threshold to go HIGH
        if (adc_value > cv->high_threshold) {
            cv->current_state = true;
        }
    }
    return cv->current_state;
}
```

### Default Thresholds

| Parameter | ADC Value | Voltage | Rationale |
|-----------|-----------|---------|-----------|
| High threshold | 128 | 2.5V | Standard Eurorack gate threshold |
| Low threshold | 77 | 1.5V | ~1V hysteresis, rejects most noise |
| Hysteresis band | 51 | 1.0V | Wide enough for slow signals |

### HAL Interface

Add ADC reading to the HAL interface:

```c
typedef struct HalInterface {
    // ... existing fields ...

    // ADC functions
    uint8_t (*adc_read)(uint8_t channel);  // Read 8-bit ADC value
} HalInterface;
```

The application layer handles threshold comparison, keeping the HAL simple.

### Sampling Rate

Read ADC once per main loop iteration (~1kHz at 1ms tick). This is fast enough for:
- Audio-rate triggers (up to ~500Hz Nyquist)
- Gate signals (typically <100Hz)
- LFO modulation (typically <20Hz)

For clock division applications, 1kHz sampling provides <1ms jitter.

### Optional: Configurable Thresholds

Thresholds could be made user-configurable via the menu system (stored in EEPROM). This is deferred to a future update but the architecture supports it.

## Consequences

### Benefits

- **Adjustable sensitivity**: Can tune thresholds for different signal sources
- **Better noise rejection**: 1V hysteresis vs 0.3V hardware Schmitt
- **Software debouncing**: Can add time-based filtering if needed
- **Diagnostic capability**: Can display actual voltage in simulator/debug
- **Future flexibility**: Could implement analog CV processing (envelope follower, slew, etc.)

### Costs

- **Increased CPU usage**: ADC read + comparison vs simple digital read
- **Slight latency**: ADC conversion takes ~13 ADC clock cycles (~13us at 1MHz ADC clock, with 8MHz system clock and /8 prescaler)
- **Code size**: ~50-100 bytes for ADC setup and threshold logic
- **RAM usage**: 4 bytes for CVInput struct

### Trade-offs

The added complexity is justified because:
1. CV input is a core feature that benefits from robustness
2. CPU overhead is minimal (<1% at 1kHz polling)
3. Code size fits within budget (~65% flash used currently)
4. Diagnostic value in simulator aids development

## Alternatives Considered

### Keep Digital Input

Continue using PB3 as digital input with hardware Schmitt trigger.

**Rejected because**: No way to adjust threshold or add hysteresis. Noise issues would require external hardware (RC filter, comparator with hysteresis).

### External Comparator

Add hardware comparator (LM393 or similar) with resistor-set thresholds.

**Rejected because**: Adds BOM cost and board space. Fixed once PCB is made. Software solution is more flexible.

### 10-bit ADC

Use full 10-bit ADC resolution instead of 8-bit.

**Rejected because**: 8-bit (20mV resolution) is adequate for gate detection. 10-bit would increase processing without meaningful benefit for this application.

### Analog Comparator

Use ATtiny85's built-in analog comparator with internal bandgap reference.

**Considered but deferred**: This could work and uses less power than ADC, but provides no voltage readout and the bandgap reference (1.1V) is lower than desired threshold. Could revisit if power consumption becomes critical.

## Open Questions

1. **Threshold tuning**: Should thresholds be user-configurable via menu? If so, what range and resolution?

2. **Debounce timing**: Should we add time-based debouncing (e.g., state must be stable for N ms)? Or is hysteresis sufficient?

3. **ADC interrupt vs polling**: Poll in main loop or use ADC interrupt for more consistent timing?

4. **Multiple channels**: If future hardware adds more analog inputs, how should the ADC be multiplexed?

## References

- [ATtiny85 Datasheet - ADC Section](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf) - Section 17
- [Schmitt Trigger](https://en.wikipedia.org/wiki/Schmitt_trigger) - Hysteresis concept
- [Doepfer A-100 Technical Details](http://www.doepfer.de/a100_man/a100t_e.htm) - Eurorack voltage standards
