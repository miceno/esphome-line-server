# SnapCap Component

SnapCap is an ESPHome TCP server component that controls a light-blocking cap device using a servo motor. It provides a protocol-based interface for controlling servo position, brightness, and light status.

## Configuration

### Basic Setup

```yaml
snapcap:
  id: snapcap_main
  servo_id: my_servo
  device_id: FLIP_FLAT
  max_degrees: 270
  brightness: 128
  position: 0
  port: 8888
```

### Parameters

- **servo_id** (Required): Reference to the servo component that controls the cap position.
- **device_id** (Optional, default: `FLIP_FLAT`): Device type identifier. Options:
  - `FLIP_FLAT` (99)
  - `FLIP_DUST` (98)
  - `FLAT_MAN` (19)
  - `FLAT_MAN_XL` (15)
  - `FLAT_MAN_L` (10)
- **max_degrees** (Optional, default: `270`): Maximum servo rotation in degrees (1–999). Used for linear mapping between degree values and servo output range (-1.0 to 1.0).
- **brightness** (Optional, default: `128`): Initial LED brightness (0–255).
- **position** (Optional, default: `0`): Initial servo position in degrees (0–max_degrees).
- **port** (Optional, default: `8888`): TCP server listening port.
- **tcp_buffer_size** (Optional, default: `256`): Ring buffer size for incoming commands.
- **tcp_terminator** (Optional, default: `\r`): Command terminator character(s).
- **tcp_timeout** (Optional, default: `300ms`): Idle timeout before closing connection.

## Degree-Based Positioning

All position values (stored internally, sent via protocol, and exposed via the Number entity) represent **physical servo degrees** relative to the configured `max_degrees`:

- **0 degrees**: Servo position fully closed
- **max_degrees**: Servo position fully open
- **Intermediate**: Proportional servo angle

For example, with `max_degrees: 270`:
- Position `0` → servo write `-1.0`
- Position `135` → servo write `0.0`
- Position `270` → servo write `1.0`

## Number Entity

Expose servo position as a Home Assistant number entity for easy slider control:

```yaml
number:
  - platform: snapcap
    snapcap_id: snapcap_main
    name: "SnapCap Servo Position"
    min_value: 0
    max_value: 270
    step: 1
```

The number entity:
- Displays current position in degrees
- Accepts values from 0 to max_degrees
- Automatically clamps out-of-range values
- Syncs bidirectionally with TCP protocol commands

## TCP Protocol

Commands use a line-based format terminated by `\r` (configurable).

### Movement Commands

| Command | Response | Effect |
|---------|----------|--------|
| `>O000` | `*O000` | Open smoothly (move to max_degrees) |
| `>o000` | `*o000` | Force open (move to max_degrees in one step) |
| `>C000` | `*C000` | Close smoothly (move to 0 degrees) |
| `>c000` | `*c000` | Force close (move to 0 degrees in one step) |
| `>APPP` | `*A000` | Abort (stop servo and detach) |

### Position Commands

| Command | Response | Note |
|---------|----------|------|
| `>NPPP` | `*NPPP` | Move to position PPP (degrees, 0–max_degrees) |
| `>M000` | `*MPPP` | Query current position (returns PPP in degrees) |

### Light Commands

| Command | Response | Effect |
|---------|----------|--------|
| `>L000` | `*L000` | Turn light on (uses current brightness) |
| `>D000` | `*D000` | Turn light off |
| `>BBBB` | `*BBBB` | Set brightness BBB (0–255) |
| `>J000` | `*JBBB` | Query brightness (returns BBB) |

### Status Commands

| Command | Response | Note |
|---------|----------|------|
| `>P000` | `*PII00` | Ping; II = device_id (2 digits) |
| `>S000` | `*SMLC` | Query status; M=motor (0/1), L=light (0/1), C=cover (0–6) |
| `>V000` | `*VVER` | Query firmware version |
| `>W000` | `*W000` | Alternate wifi/serial mode (ack only) |

## Example: Full Configuration

```yaml
esphome:
  name: snapcap_device

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

uart:
  baud_rate: 0

servo:
  - id: cap_servo
    pin: GPIO5
    min_angle: -90.0
    max_angle: 90.0
    auto_detach_time: 0ms

snapcap:
  id: snapcap_main
  servo_id: cap_servo
  device_id: FLIP_FLAT
  max_degrees: 270
  brightness: 128
  position: 0
  port: 8888

number:
  - platform: snapcap
    snapcap_id: snapcap_main
    name: "SnapCap Servo Position"
    unit_of_measurement: "°"
    min_value: 0
    max_value: 270
    step: 1
```

## Notes

- Position values in `>N` commands and `*M` responses are **clamped** to `0..max_degrees`.
- The Number entity slider automatically respects `max_degrees` as its maximum.
- If servo is not configured, commands return `*ERR\r\n`.
- The component publishes servo position to Home Assistant on each command via the Number entity (if configured).

