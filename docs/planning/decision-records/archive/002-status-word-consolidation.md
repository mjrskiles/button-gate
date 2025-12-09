# ADR-002: Status Word Consolidation

## Status

Accepted

## Date

2025-12-08

## Context

The ATtiny85 has only 512 bytes of SRAM. As Gatekeeper expands with new features (FSM, menu system, CV processing, LED animations), memory efficiency becomes critical.

The current codebase uses individual `bool` fields for state tracking in structs like `Button` (7 bools), `CVOutput` (3 bools), and the proposed `EventProcessor` (8 bools). While readable, this approach wastes memory—each bool consumes a full byte on AVR.

Additionally, checking or modifying multiple related flags requires multiple operations, and there's no established convention for flag management across modules.

## Decision

Consolidate multiple boolean fields into `uint8_t` status words with named bit definitions.

**Convention:**

```c
// Bit definitions: MODULE_FLAGNAME
#define BTN_PRESSED      (1 << 0)
#define BTN_RISE         (1 << 1)
// ...

// Status word in struct
typedef struct {
    uint8_t status;    // Flags (see BTN_* defines)
    // ... other fields
} Button;
```

**Common macros** (`include/utility/status.h`):

```c
#define STATUS_SET(status, mask)    ((status) |= (mask))
#define STATUS_CLR(status, mask)    ((status) &= ~(mask))
#define STATUS_ANY(status, mask)    (((status) & (mask)) != 0)
#define STATUS_ALL(status, mask)    (((status) & (mask)) == (mask))
#define STATUS_PUT(status, mask, v) ((v) ? STATUS_SET(status,mask) : STATUS_CLR(status,mask))
```

**Modules to refactor:**

| Module | Bools → Status Word | Savings |
|--------|---------------------|---------|
| Button | 7 → 1 byte | 6 bytes |
| CVOutput | 3 → 1 byte | 2 bytes |
| EventProcessor | 8 → 1 byte | 7 bytes |
| CVInput | 2 → 1 byte (+ config flags) | 1 byte |
| FSM | 1 → packed into state byte | 1 byte |

**Total savings: ~18 bytes (3.5% of SRAM)**

## Consequences

**Benefits:**

- **Memory savings**: 18 bytes recovered for other uses
- **Atomic operations**: Set/clear multiple flags in one instruction
- **Efficient checks**: Test multiple conditions with single AND (`STATUS_ALL`)
- **Embedded idiom**: Follows industry-standard microcontroller patterns
- **Consistent API**: Common macros across all modules
- **Future-proof**: Reserved bits allow adding flags without struct changes

**Costs:**

- **Readability**: `STATUS_ANY(btn->status, BTN_PRESSED)` is slightly less obvious than `btn->pressed`
- **Refactoring effort**: Existing code and tests must be updated
- **Documentation**: Bit assignments must be clearly documented

**Migration:**

Existing tests will verify behavior is unchanged. Struct size assertions will confirm memory savings.

## Alternatives Considered

**1. C Bit-fields**

```c
struct { uint8_t pressed:1; uint8_t rising:1; /* ... */ };
```

Rejected: Memory layout is compiler-dependent. Cannot use bitwise operators for bulk operations.

**2. Keep bools, optimize elsewhere**

Rejected: Bool consolidation is straightforward with clear benefits. Other optimizations are harder.

**3. Larger status words (uint16_t/uint32_t)**

Rejected: 8 bits is sufficient for all current structs. Larger words waste RAM on 8-bit AVR.

## Open Questions

None—design is fully specified in FDP-006.

## References

- [FDP-006: Status Word and Bitmask Consolidation](../feature-designs/FDP-006-status-bitmasks.md)
- [AVR Libc: Bit manipulation](https://www.nongnu.org/avr-libc/user-manual/group__avr__sfr.html)
