# Gatekeeper Roadmap

> Last updated: 2025-12-15

## Milestone Philosophy

Each milestone delivers a **working product** - something you can actually use, not just a completed feature. Milestones are sequenced by dependencies and hardware availability.

**Notation:**
- `→` means "unlocks" or "enables"
- Items marked `[SIM]` are simulator-specific
- Items marked `[HW]` require real hardware

---

## M0: Core Architecture ✓

**Theme:** Prove the firmware architecture works
**Done when:** All modes work in simulator with full test coverage

| Deliverable | Status |
|-------------|--------|
| Table-driven FSM engine | ✓ Complete |
| Three-level FSM hierarchy (Top/Mode/Menu) | ✓ Complete |
| Event processor (gestures, edge detection) | ✓ Complete |
| Mode handlers (Gate, Trigger, Toggle, Divide, Cycle) | ✓ Complete |
| HAL abstraction (production, mock, sim) | ✓ Complete |
| Neopixel LED driver with animations | ✓ Complete |
| EEPROM settings with validation | ✓ Complete |
| Factory reset detection | ✓ Complete |
| 144 unit tests passing | ✓ Complete |

**Unlocks:** M1 (firmware ready for hardware)

---

## M1: Rev2 Hardware

**Theme:** Real hardware bring-up
**Done when:** Module works in Eurorack case with real buttons and CV

### Critical Path
```
PCB Assembly → Power-on Test → Button/LED Test → CV I/O Calibration → Integration Test
```

| Deliverable | Status | Notes |
|-------------|--------|-------|
| Rev2 PCB design | ✓ Complete | KiCad files ready |
| PCB fabrication | In progress | Ordered, awaiting delivery |
| Component sourcing | ✓ Complete | All parts on hand |
| Power-on test | Not started | 5V/3.3V rails, current draw |
| Button wiring test | Not started | Debounce validation |
| Neopixel test | Not started | WS2812B timing on real LEDs |
| CV input calibration | Not started | ADC thresholds, hysteresis |
| CV output test | Not started | Gate/trigger signal levels |
| Full integration test | Not started | All modes in Eurorack |

### Supporting Work
| Deliverable | Status | Notes |
|-------------|--------|-------|
| Watchdog timer (FDP-008) | ✓ Complete | HAL support, enabled in main loop |
| Fuse configuration | Not started | 8MHz internal, BOD settings |
| Programming jig | Not started | pogo pins or test clips |

---

## M2: Simulator Enhancement

**Theme:** Better development and testing tools
**Done when:** External frontend can control simulator via socket with gamepad input

### Phase 1: CV Sources ✓
| Deliverable | Status |
|-------------|--------|
| CV source abstraction | ✓ Complete |
| Manual CV control | ✓ Complete |
| LFO (sine, tri, saw, square, random) | ✓ Complete |
| ADSR envelope | ✓ Complete |
| Wavetable support | ✓ Complete |
| Keyboard LFO cycling ('l' key) | ✓ Complete |

### Phase 2: Socket Server ✓
| Deliverable | Status |
|-------------|--------|
| Unix domain socket server | ✓ Complete |
| NDJSON command protocol | ✓ Complete |
| 60Hz state streaming | ✓ Complete |
| Button commands | ✓ Complete |
| CV source commands | ✓ Complete |

### Phase 3: Frontend (Ironclad) [SIM]
| Deliverable | Status | Notes |
|-------------|--------|-------|
| Python frontend scaffold | Not started | pygame-based |
| Gamepad input (Xbox controller) | Not started | pygame joystick API |
| Visual display | Not started | LED rendering, state display |
| Recording/playback | Not started | Session capture |

---

## M3: Production Polish

**Theme:** Ready for real-world use
**Done when:** Module survives a week of actual use

### Reliability
| Deliverable | Status | Notes |
|-------------|--------|-------|
| Extended runtime test | Not started | 24+ hour soak test |
| Power cycle stress test | Not started | Rapid on/off cycles |
| CV input noise testing | Not started | Real Eurorack signals |
| EEPROM wear analysis | Not started | Write cycle counting |

### User Experience
| Deliverable | Status | Notes |
|-------------|--------|-------|
| LED feedback tuning | Not started | Brightness, colors |
| Button feel validation | Not started | Debounce timing |
| Menu timeout tuning | Not started | Currently 60s |
| Mode-specific defaults | Not started | Per-mode sensible values |

### Documentation
| Deliverable | Status | Notes |
|-------------|--------|-------|
| User manual | Not started | Operation guide |
| Build guide | Not started | Assembly instructions |
| Calibration procedure | Not started | CV input adjustment |

---

## Dependency Graph

```
M0: Core Architecture ✓
    │
    ├── M1: Rev2 Hardware ──→ M3: Production Polish
    │
    └── M2: Simulator Enhancement
            │
            └── M3: Production Polish (informed by sim testing)
```

---

## Current Focus

Active work items:

- [x] **Socket server (FDP-011 Phase 2)** - Complete
- [ ] **Rev2 PCB arrival** - Waiting on fabrication
- [ ] **Ironclad frontend** - Next simulator work

---

## Recently Completed

- [x] Socket server with NDJSON protocol - December 2025
- [x] CV source generators (LFO, envelope, wavetable) - December 2025
- [x] Simulator headless architecture (FDP-009) - December 2025
- [x] Watchdog timer support (FDP-008) - December 2025
- [x] LED feedback controller with animations - December 2025
- [x] Mode handlers with menu integration - December 2025
- [x] Table-driven FSM engine - December 2025
- [x] Analog CV input with hysteresis (ADR-004) - December 2025

---

## Notes

### Design Principles
- **Minimal footprint** - ATtiny85 has 8KB flash, 512B RAM
- **No dynamic allocation** - All memory statically allocated
- **Testable without hardware** - Mock HAL enables full unit testing
- **Simulator-first development** - Iterate fast before touching hardware

### Constraints
- Flash budget: ~68% used (5.5KB of 8KB)
- RAM budget: ~24% used (124B of 512B)
- Single CV input (analog via ADC)
- Two buttons, two Neopixel LEDs
