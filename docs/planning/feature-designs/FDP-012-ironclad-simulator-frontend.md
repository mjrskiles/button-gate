---
type: fdp
id: FDP-012
status: proposed
created: 2025-12-15
modified: 2025-12-15
supersedes: null
superseded_by: null
obsoleted_by: null
related: [FDP-011]
---

# FDP-012: Ironclad - Simulator Frontend

## Status

Proposed

## Problem Statement

The gatekeeper simulator currently supports keyboard input and socket control, but:

1. **Development environment**: User develops on RPi 5 but interacts via SSH from Windows PC
2. **Input limitations**: Keyboard is clunky for real-time control; want gamepad (Xbox controller)
3. **Visual feedback**: Terminal UI works but a graphical display would be more intuitive
4. **Remote operation**: Unix domain sockets don't cross network boundaries

**Goal**: Create "Ironclad" (Fe = Iron) - a frontend that provides gamepad control and visual feedback, working seamlessly in a remote development setup.

## Architecture Options

### Option A: TCP Socket Extension (Recommended)

Extend the simulator to accept TCP connections in addition to Unix domain sockets.

```
┌─────────────────────┐         TCP/IP          ┌─────────────────────┐
│  Windows PC         │◄──────────────────────►│  Raspberry Pi 5     │
│                     │                         │                     │
│  ┌───────────────┐  │    NDJSON over TCP      │  ┌───────────────┐  │
│  │   Ironclad    │  │◄───────────────────────►│  │ gatekeeper-sim│  │
│  │   (Python)    │  │                         │  │  --socket-tcp │  │
│  │               │  │                         │  │    :9742      │  │
│  │  Xbox Ctrl ◄──┤  │                         │  └───────────────┘  │
│  │  pygame UI    │  │                         │                     │
│  └───────────────┘  │                         │                     │
└─────────────────────┘                         └─────────────────────┘
```

**Pros:**
- Reuses existing NDJSON protocol exactly
- Minimal sim changes (~50 lines)
- Native Windows app with full controller support
- Low latency (<1ms on LAN)
- Frontend can run anywhere on the network

**Cons:**
- Opens a network port (security consideration)
- Need to handle connection drops gracefully

**Implementation:**
```bash
# On RPi
./gatekeeper-sim --socket-tcp 9742

# On Windows
python ironclad.py --host 192.168.1.100 --port 9742
```

### Option B: WebSocket Bridge + Web UI

Add a small WebSocket bridge and build a browser-based frontend.

```
┌─────────────────────┐         HTTP/WS         ┌─────────────────────┐
│  Windows PC         │◄──────────────────────►│  Raspberry Pi 5     │
│                     │                         │                     │
│  ┌───────────────┐  │                         │  ┌───────────────┐  │
│  │   Browser     │  │    WebSocket :8080      │  │  ws-bridge.py │  │
│  │   (Canvas +   │◄─┼───────────────────────►│  │       ▲       │  │
│  │   Gamepad API)│  │                         │  │       │ Unix  │  │
│  │               │  │                         │  │       ▼ sock  │  │
│  │  Xbox Ctrl ◄──┤  │                         │  │ gatekeeper-sim│  │
│  └───────────────┘  │                         │  └───────────────┘  │
└─────────────────────┘                         └─────────────────────┘
```

**Pros:**
- Works in any browser, no install needed
- Gamepad API supports Xbox controllers natively
- Easy to share/demo (just open URL)
- Could host on GitHub Pages with sim running locally

**Cons:**
- Extra bridge component
- Browser gamepad API has quirks
- More complex to develop (HTML/JS/CSS)
- Slightly higher latency (WebSocket overhead)

### Option C: SSH Tunnel + Native Client

Keep Unix socket, tunnel it over SSH, run client on Windows via WSL2.

```
┌─────────────────────────────────────────────────────────────────────┐
│  Windows PC                                                          │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  WSL2 (Ubuntu)                                                  │ │
│  │  ┌───────────────┐      ┌──────────────────────────────────┐   │ │
│  │  │   Ironclad    │◄────►│ SSH tunnel to RPi Unix socket    │   │ │
│  │  │   (Python)    │      │ socat UNIX:/tmp/gk.sock ...      │   │ │
│  │  └───────────────┘      └──────────────────────────────────┘   │ │
│  │         ▲                                                       │ │
│  │         │ (controller passthrough?)                             │ │
│  └─────────┼──────────────────────────────────────────────────────┘ │
│            │                                                         │
│   Xbox Controller                                                    │
└─────────────────────────────────────────────────────────────────────┘
```

**Pros:**
- No sim changes needed
- Encrypted by default

**Cons:**
- Complex setup (WSL2, SSH tunnels, socat)
- Controller passthrough to WSL2 is painful
- Display from WSL2 requires X server or WSLg
- Too many moving parts

### Option D: VNC / Remote Desktop

Run pygame frontend on RPi, view via VNC.

**Pros:**
- Simple conceptually
- Controller could connect to Windows and use vJoy/ViGEm to forward

**Cons:**
- High latency (display encoding/decoding)
- Bandwidth heavy
- Input lag makes real-time control frustrating
- Controller forwarding is complex

### Option E: Hybrid - TCP Socket + Optional Web Dashboard

Combine A and B: TCP socket is primary, optional web dashboard for monitoring.

```
                                        ┌─────────────────────┐
                                   ┌───►│  Web Dashboard      │ (view-only)
                                   │    │  :8080              │
┌─────────────────────┐            │    └─────────────────────┘
│  Windows PC         │            │
│  ┌───────────────┐  │    TCP     │    ┌─────────────────────┐
│  │   Ironclad    │◄─┼───────────►│───►│  gatekeeper-sim     │
│  │   (Python)    │  │   :9742    │    │  --socket-tcp 9742  │
│  │  Xbox Ctrl    │  │            │    │  --web-dashboard    │
│  └───────────────┘  │            │    └─────────────────────┘
└─────────────────────┘            │
                                   │    ┌─────────────────────┐
                                   └───►│  Phone/Tablet       │ (view-only)
                                        │  Browser            │
                                        └─────────────────────┘
```

**Pros:**
- Best of both worlds
- Control from native app, monitor from anywhere
- Could share dashboard URL for demos

**Cons:**
- More to implement

## Recommendation

**Option A (TCP Socket Extension)** for the primary implementation, with Option E's web dashboard as a future enhancement.

Rationale:
- Minimal changes to existing sim code
- Reuses proven NDJSON protocol
- Best controller support (native Python + pygame/inputs library)
- Lowest latency for real-time control
- Clean separation: sim on RPi, frontend on Windows

## Controller Mapping

### Xbox Controller Layout

```
        [LB]                              [RB]
        CV Cable Toggle                   CV Source Cycle
                                          (manual→LFO→env)
   [LT]                                        [RT]
   CV Voltage                                  Gate/Trigger
   (analog 0-255)                              (envelope gate)

         ┌───┐                           ┌───┐
      [L]│   │                           │   │[R]
         └───┘                           └───┘
     LFO Freq/Rate                    (reserved)
     (when LFO active)

              [View]     [Menu]
              Reset      Sim Menu
                  [Xbox]
                  (reserved)

                 ┌───┐
                 │ Y │  Mode: Trigger
            ┌───┐└───┘┌───┐
            │ X │     │ B │  Mode: Toggle / Button B tap
            └───┘┌───┐└───┘
                 │ A │  Mode: Gate / Button A tap
                 └───┘

                 [D-Pad]
              ┌────┬────┐
              │ ↑  │    │  Up: Mode Divide
         ┌────┼────┼────┤
         │ ←  │    │ →  │  Left/Right: Menu page nav
         └────┼────┼────┘
              │ ↓  │       Down: Mode Cycle
              └────┘
```

### Mapping Table

| Input | Action | Notes |
|-------|--------|-------|
| **LT (Left Trigger)** | CV voltage (analog) | 0-255 proportional to trigger position |
| **RT (Right Trigger)** | Envelope gate | Pressed = gate on, triggers ADSR |
| **LB (Left Bumper)** | Toggle CV cable | Simulates plugging/unplugging CV |
| **RB (Right Bumper)** | Cycle CV source | Manual → LFO (sine) → LFO (square) → Envelope → ... |
| **Left Stick Y** | LFO frequency | When LFO active, adjusts rate (0.1-10Hz) |
| **Left Stick X** | LFO shape morph | Blend between waveforms (stretch goal) |
| **Left Stick Click** | Toggle fast mode | Speed up/slow down simulation |
| **A Button** | Button A tap | Menu: cycle value |
| **B Button** | Button B tap | Primary gate button |
| **X Button** | Mode: Toggle | Quick mode select |
| **Y Button** | Mode: Trigger | Quick mode select |
| **D-Pad Up** | Mode: Divide | Quick mode select |
| **D-Pad Down** | Mode: Cycle | Quick mode select |
| **D-Pad Left/Right** | Menu page nav | When in menu |
| **View Button** | Reset simulation | Time and state reset |
| **Menu Button** | Toggle sim menu | Enter/exit menu mode |
| **Right Stick** | Reserved | Future: pan/zoom display? |

### Quick Mode Selection

Instead of the gesture-based mode change (Hold B + Hold A), controller allows instant mode selection:

| Button | Mode |
|--------|------|
| A | Gate (green) |
| X | Toggle (orange) |
| Y | Trigger (cyan) |
| D-Up | Divide (magenta) |
| D-Down | Cycle (blue) |

This is a **frontend shortcut** - it sends the appropriate commands to switch modes without going through the gesture system.

## Visual Design

### Main Display

```
╔══════════════════════════════════════════════════════════════╗
║  IRONCLAD - Gatekeeper Simulator            [Connected ●]    ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║     ┌─────────┐                     ┌─────────┐              ║
║     │  MODE   │                     │ ACTIVE  │              ║
║     │  ████   │                     │  ████   │              ║
║     │  LED    │                     │  LED    │              ║
║     └─────────┘                     └─────────┘              ║
║        GATE                           HIGH                   ║
║                                                              ║
║   ┌─────────────────────────────────────────────────────┐    ║
║   │                    CV INPUT                         │    ║
║   │  ═══════════════════════●════                       │    ║
║   │  0V              2.8V                          5V   │    ║
║   │                                                     │    ║
║   │  Source: LFO (sine @ 2.0 Hz)          [Cable: IN]   │    ║
║   └─────────────────────────────────────────────────────┘    ║
║                                                              ║
║   State: PERFORM    Mode: GATE    Time: 12,345 ms            ║
║                                                              ║
║   ┌──────────────────────┐  ┌──────────────────────────┐     ║
║   │ [A] ○  Released      │  │ [B] ●  PRESSED           │     ║
║   └──────────────────────┘  └──────────────────────────┘     ║
║                                                              ║
╠══════════════════════════════════════════════════════════════╣
║  LT: CV  [███████░░░] 178   RT: Gate [░░░░░░░░░░] Off        ║
║  LB: Cable IN    RB: Source LFO    View: Reset   Menu: Menu  ║
╚══════════════════════════════════════════════════════════════╝
```

### Features

1. **LED Visualization**: Actual RGB colors rendered as colored blocks
2. **CV Voltage Bar**: Real-time voltage with threshold markers
3. **CV Source Indicator**: Shows current source type and parameters
4. **Cable Status**: Visual indicator for CV "plugged in" state
5. **Button State**: Shows which buttons are currently pressed
6. **Controller HUD**: Bottom bar shows current controller state
7. **Connection Status**: Shows TCP connection health

## Implementation Plan

### Phase 1: TCP Socket Support (Sim Side)

Add `--socket-tcp [port]` flag to gatekeeper-sim:

```c
// In sim_main.c argument parsing
} else if (strcmp(argv[i], "--socket-tcp") == 0) {
    socket_mode = SOCKET_TCP;
    if (i + 1 < argc && argv[i + 1][0] != '-') {
        tcp_port = atoi(argv[++i]);
    } else {
        tcp_port = 9742;  // Default: "GK" in ASCII
    }
}
```

Extend socket_server.c to support both Unix and TCP:

```c
typedef enum {
    SOCKET_TYPE_UNIX,
    SOCKET_TYPE_TCP
} SocketType;

SocketServer* socket_server_create_tcp(uint16_t port);
```

### Phase 2: Basic Frontend

Python frontend with:
- TCP socket client
- pygame display
- Keyboard fallback controls

```
tools/ironclad/
├── ironclad.py          # Main entry point
├── connection.py        # TCP socket client
├── display.py           # pygame rendering
├── controller.py        # Gamepad input handling
├── state.py             # State management
└── config.py            # Keybindings, settings
```

### Phase 3: Controller Integration

Xbox controller via pygame or `inputs` library:

```python
import pygame

pygame.joystick.init()
controller = pygame.joystick.Joystick(0)
controller.init()

# In event loop
lt_value = controller.get_axis(4)  # Left trigger
rb_pressed = controller.get_button(5)  # Right bumper
```

### Phase 4: Polish

- Configuration file for custom bindings
- Recording/playback of sessions
- Multiple controller support
- Haptic feedback (rumble on gate trigger)

## Protocol Extensions

New commands needed for frontend convenience:

```json
// Direct mode set (bypasses gesture)
{"cmd": "set_mode", "mode": "trigger"}

// CV cable simulation
{"cmd": "cv_cable", "connected": false}

// Query current state (for reconnection)
{"cmd": "get_state"}

// Response to get_state
{"type": "full_state", "mode": "GATE", "top_state": "PERFORM", ...}
```

## Security Considerations

TCP socket opens network access. Mitigations:

1. **Bind to localhost by default**: `--socket-tcp` binds to 127.0.0.1
2. **Explicit network flag**: `--socket-tcp-listen 0.0.0.0:9742` for network
3. **No authentication** (acceptable for dev tool on trusted LAN)
4. **Firewall**: Document that users should firewall the port

For extra security (future):
- Simple shared secret in handshake
- TLS support via stunnel or similar

## Open Questions

1. **Controller library**: pygame vs `inputs` vs XInput directly?
   - pygame: Cross-platform, well-documented, but pygame overhead
   - inputs: Lightweight, but less maintained
   - XInput: Windows-only, but best Xbox support

2. **Display framework**: pygame vs tkinter vs web?
   - pygame: Good for real-time, gamedev feel
   - tkinter: Simpler, but uglier
   - web: Most flexible, but more complex

3. **Port number**: 9742 ("GK" in ASCII-ish) or something else?

4. **Haptic feedback**: Worth implementing rumble on events?

## Alternatives Considered

See Architecture Options section above. VNC and SSH tunneling were rejected due to latency and complexity. WebSocket remains a viable future enhancement for browser-based monitoring.

## References

- [FDP-011: Simulator Socket Architecture](FDP-011-simulator-socket-architecture.md)
- [pygame joystick docs](https://www.pygame.org/docs/ref/joystick.html)
- [Gamepad API (Web)](https://developer.mozilla.org/en-US/docs/Web/API/Gamepad_API)
- [inputs library](https://github.com/zeth/inputs)

---

## Addenda
