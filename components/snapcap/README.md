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

### `device_id` accepted values

`device_id` accepts either an enum name or its numeric protocol value.

| Name | Numeric value |
|------|----------------|
| `FLAT_MAN_L` | `10` |
| `FLAT_MAN_XL` | `15` |
| `FLAT_MAN` | `19` |
| `FLIP_DUST` | `98` |
| `FLIP_FLAT` | `99` |

Examples:

```yaml
# Enum form (recommended)
snapcap:
  id: snapcap_main
  servo_id: my_servo
  device_id: FLIP_FLAT
```

```yaml
# Numeric form
snapcap:
  id: snapcap_main
  servo_id: my_servo
  device_id: 99
```

Name matching is case-insensitive; `-` and spaces are normalized to `_`.

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
- **position** (Optional, default: `0`): Initial servo position in degrees. Values are clamped at runtime to `0..max_degrees`.
- **port** (Optional, default: `8888`): TCP server listening port.
- **tcp_buffer_size** (Optional, default: `256`): Ring buffer size for incoming commands. Must be a power of two.
- **tcp_terminator** (Optional, default: `\r`): Command terminator character(s).
- **tcp_timeout** (Optional, default: `300ms`): Idle timeout for incomplete buffered input. When the terminator is not received in time, the partial command is discarded or transformed by `tcp_timeout_lambda`.
- **tcp_timeout_lambda** (Optional): Lambda that receives a timed-out partial command and returns a replacement command string to process.

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
    step: 1
```

The number entity:
- Displays current position in degrees
- Has a fixed minimum of `0`
- Uses the parent `snapcap.max_degrees` value as its maximum
- Accepts values from `0` to `max_degrees`
- Rounds incoming values to the nearest integer and clamps out-of-range values
- Syncs bidirectionally with TCP protocol commands

## TCP Protocol

Commands use a line-based format terminated by `\r` by default (configurable via `tcp_terminator`). Commands begin with `>` and responses begin with `*`. Responses shown below are sent with `\r\n` line endings.

### Movement Commands

| Command | Response | Effect |
|---------|----------|--------|
| `>O000` | `*O000` | Open (move to `max_degrees`) |
| `>o000` | `*o000` | Force open (currently behaves the same as `>O000`) |
| `>C000` | `*C000` | Close (move to 0 degrees) |
| `>c000` | `*c000` | Force close (currently behaves the same as `>C000`) |
| `>A000` | `*A000` | Abort (detach servo) |

### Position Commands

| Command | Response | Note |
|---------|----------|------|
| `>NPPP` | `*NPPP` | Move to position `PPP` (degrees, clamped to `0..max_degrees`) |
| `>M000` | `*MPPP` | Query current position (returns `PPP` in degrees) |

### Light Commands

| Command | Response | Effect |
|---------|----------|--------|
| `>L000` | `*L000` | Turn light on |
| `>D000` | `*D000` | Turn light off |
| `>BXXX` | `*BXXX` | Set brightness `XXX` (0–255) |
| `>J000` | `*JXXX` | Query brightness (returns `XXX`) |

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
    step: 1
```

## Notes

- Position values in `>N` commands and `*M` responses are **clamped** to `0..max_degrees`.
- The Number entity slider automatically respects `max_degrees` as its maximum.
- Servo-dependent commands (`>O`, `>o`, `>C`, `>c`, `>A`, `>N`, `>S`) return `*ERR\r\n` if no servo is configured.
- The component publishes servo position to Home Assistant on each command via the Number entity (if configured).
