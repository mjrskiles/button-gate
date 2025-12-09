# ADR-001: Rev2 Hardware and Firmware Architecture

## Status

Accepted

## Date

2025-12-07

## Context

The current Gatekeeper module (rev1) is a simple 5V digital gate output
controlled by a button or CV input. It supports three modes (Gate, Pulse,
Toggle) selectable via a 5-tap + 1-second-hold sequence.

The design works but has limitations:

1. Single button makes mode selection cumbersome
2. Three discrete LEDs consume 3 GPIOs
3. No persistent mode memory (resets to Gate on power cycle)
4. Limited functionality for a Eurorack context
5. CV input only acts as a simple gate/trigger

Rev2 aims to make the module more useful and interesting while remaining
an accessible open-source example of Eurorack firmware design.

## Decision

### Hardware Changes

**GPIO Allocation (ATtiny85)**

| Pin | Function | Direction | Notes |
|-----|----------|-----------|-------|
| PB5 | RESET | - | Preserved for ISP programming |
| PB4 | Button B | Input | Secondary button for value/action |
| PB3 | CV Input | Input | Clock, gate, or trigger input |
| PB2 | Button A | Input | Primary button for menu/mode |
| PB1 | Digital Out | Output | Directly to buffer circuit |
| PB0 | Neopixel Data | Output | WS2812B data line |

**LED Changes**

Replace three discrete LEDs with two WS2812B addressable RGB LEDs:

- Neopixel A: Mode/page indicator (color encodes mode or menu page)
- Neopixel B: Value/activity indicator (shows setting values, output state)

This frees two GPIOs for the second button.

**Output Buffer**

The digital output will drive a buffer circuit (transistor or op-amp) that
also illuminates an LED indicator. This provides output status without
consuming a GPIO.

**Clock Speed**

Increase from 1 MHz to 8 MHz internal oscillator (fuse change only). This
provides reliable WS2812B timing and more headroom for processing.

### Firmware Changes

**Two-Button Interface**

Replace the 5-tap + hold sequence with a two-button system:

- Normal operation: Buttons perform mode-specific actions
- Menu mode: Hold Button A to enter, tap A to change page, tap B to cycle values
- Release Button A to exit menu and save settings

**Persistent Settings (EEPROM)**

Store configuration in the ATtiny85's 512-byte EEPROM:

- Current mode
- Per-mode settings (division ratio, pattern parameters, CV function)
- Settings load on startup, save on menu exit

Use `eeprom_update_byte()` to minimize write cycles.

**State Machine Architecture**

Replace switch-case state management with a table-driven finite state machine:

- States and events defined as enums
- Transition table maps (state, event) -> (next_state, action)
- Transition table stored in PROGMEM to conserve RAM
- Separate FSMs for top-level (Running/Menu) and per-mode behavior

**New Modes**

Extend beyond Gate/Pulse/Toggle with:

- Clock Divider: Divide incoming clock by configurable ratio
- Additional modes TBD (Euclidean rhythms, probability gate, burst generator)

**CV Input Functions**

CV input behavior becomes configurable per mode:

- Clock source (for divider/rhythm modes)
- Reset/sync input
- Gate input (for gate mode)
- Configurable via menu

### Menu System

Visual feedback via Neopixels:

- Neopixel A color indicates current page
- Neopixel B shows current value (color, brightness, or pulse count)

Pages (preliminary):

1. Mode selection
2. CV input function
3. Mode-specific parameter 1
4. Mode-specific parameter 2

## Consequences

**Benefits**

- More intuitive user interface (two buttons vs tap sequence)
- Richer visual feedback (RGB colors vs binary LED patterns)
- Persistent settings across power cycles
- Extensible mode system
- Cleaner codebase via proper state machine architecture
- Frees GPIO for second button without adding pins

**Costs**

- Slightly higher BOM cost (WS2812B vs discrete LEDs)
- More complex firmware (state machine, menu system, EEPROM)
- Requires 8 MHz operation for reliable Neopixel timing
- Additional code space for WS2812B driver

**Migration**

Rev1 firmware remains functional on rev1 hardware. Rev2 firmware requires
rev2 hardware (different GPIO assignments, Neopixels).

## Alternatives Considered

**Shift Register for LEDs**

Using a 74HC595 to drive multiple LEDs from fewer GPIOs was considered.
However, this requires 2-3 GPIOs (data, clock, latch) and doesn't provide
the color-coding benefits of RGB LEDs. WS2812B uses only 1 GPIO and
enables richer feedback.

**Charlieplexing**

Could drive more LEDs with fewer pins but doesn't reduce pin count for
our 2-LED minimum and adds software complexity without the color benefit.

**Keep Single Button**

Retaining the single-button interface with complex gestures was rejected
as unintuitive. The 5-tap + hold sequence is difficult to discover and
error-prone.

**I2C LED Driver**

Using an I2C LED driver (PCA9685 or similar) would require 2 pins (SDA/SCL)
and external IC. More complex than WS2812B for limited benefit at this scale.

## Open Questions

1. **Menu timeout behavior**: Should the menu auto-exit after inactivity?
   If so, should it save or discard changes?

2. **Module behavior during menu**: Should the module continue processing
   CV and producing output while in menu mode, or pause?

3. **Full mode list**: Which modes to implement for initial release?
   Candidates: Gate, Pulse, Toggle, Divider, Euclidean, Probability, Burst.

4. **CV function options per mode**: What CV input functions make sense
   for each mode? (Clock, Reset, Gate, Rotate, etc.)

5. **Button functions during normal operation**: What should A and B do
   in each mode when not in menu?

6. **Reset to defaults**: Should holding both buttons trigger a factory
   reset? What feedback to provide?

7. **State machine implementation**: Table-driven (flat) vs hierarchical
   (nested FSMs for menu and each mode)?

8. **WS2812B library**: Use existing light_ws2812 library or write minimal
   driver?

9. **Division ratio range**: What clock division ratios to support?
   Powers of 2 only (/2, /4, /8, /16) or arbitrary (/2 through /16)?

10. **Euclidean parameters**: If implemented, what ranges for K (hits)
    and N (steps)?

## References

- [WS2812B Datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf)
- [light_ws2812 Library](https://github.com/cpldcpu/light_ws2812)
- [ATtiny85 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf)
- [Make Noise 0-Coast Manual](https://www.makenoisemusic.com/manuals/0-coast_manual.pdf) - Menu system inspiration
- [Euclidean Rhythms Paper](http://cgm.cs.mcgill.ca/~godfried/publications/banff.pdf) - Toussaint, 2005
