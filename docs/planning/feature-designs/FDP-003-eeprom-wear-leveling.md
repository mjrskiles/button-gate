# FDP-003: EEPROM Wear Leveling

## Status

Deferred

## Summary

Implement wear leveling for EEPROM settings storage to extend the lifespan of the ATtiny85's EEPROM beyond its rated 100,000 write cycles.

## Motivation

While the ATtiny85 EEPROM is rated for 100,000 write cycles per byte, heavy use patterns (frequent setting changes) could theoretically approach this limit over the module's lifetime. Wear leveling distributes writes across multiple EEPROM locations to extend overall lifespan.

## Detailed Design

*To be designed when/if needed.*

Potential approaches:
1. **Ring buffer**: Rotate settings storage across N slots, use sequence number to find current
2. **Copy-on-write**: Write to alternating locations, track which is active
3. **Per-byte tracking**: Track write counts and migrate hot bytes

## Why Deferred

For typical use (settings changed a few times per session, a few sessions per week), the EEPROM will last decades. Implementation complexity is not justified for current use patterns.

Revisit if:
- Use cases emerge requiring frequent automated setting changes
- Users report EEPROM failures in the field
- Settings struct grows significantly larger

## References

- [ATtiny85 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf) - Section 5.3 EEPROM Data Memory
- [AVR101: High Endurance EEPROM Storage](https://ww1.microchip.com/downloads/en/AppNotes/doc2526.pdf)
