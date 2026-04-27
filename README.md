# ESPHome TCP Components (ESP8266)

This repository contains external ESPHome components for TCP-based device protocols on ESP8266.

## Project Status

- `tcp_server` is the maintained base component in this repo.
- `rolloffino` and `snapcap` build on top of `tcp_server`.
- `line_server` is legacy in this repository and is not the focus for new work.

## Attribution

This project is inspired by:

- [`esphome-line-server`](https://github.com/gstos/esphome-line-server) by `gstos`
- [`esphome-stream-server`](https://github.com/oxan/esphome-stream-server) by `oxan`

## Requirements

- ESPHome `>= 2022.3.0`.
- Target board family: ESP8266

## Installation

```yaml
external_components:
  - source: github://miceno/esphome-line-server
    components: [tcp_server, rolloffino, snapcap]
```

## Components

| Component | Role |
|---|---|
| `tcp_server` | Base TCP server runtime (socket handling, ring buffer, command framing, timeout flush). |
| `rolloffino` | Rolloffino roof controller protocol implementation on top of `tcp_server`. |
| `snapcap` | SnapCap protocol implementation on top of `tcp_server` with servo/light control. |

## `tcp_server` (base)

`tcp_server` is a reusable base component for line-delimited TCP command protocols.
It is used by `rolloffino` and `snapcap` for shared networking behavior.

### Shared TCP Options

These options are inherited by derived components (`snapcap`, `rolloffino`):

| Key | Type | Default | Description |
|---|---|---|---|
| `port` | integer | `8888` | TCP listening port. |
| `tcp_buffer_size` | power-of-2 int | `256` | Ring buffer size for incoming TCP data. |
| `tcp_terminator` | string (<= 4 bytes) | `"\r"` | Terminator used to split commands from the input stream. |
| `tcp_timeout` | duration | `300ms` | Idle timeout for partial data in buffer. |
| `tcp_timeout_lambda` | lambda | unset | Optional lambda to transform or discard timed-out partial input. |

### Timeout Lambda Behavior

When `tcp_timeout` is reached and no terminator arrived:

- If `tcp_timeout_lambda` is set, the partial buffer is passed to the lambda.
- If the lambda returns a non-empty string, that returned command is processed.
- If it returns an empty string, partial input is discarded.
- Without a lambda, partial input is discarded.

Example:

```yaml
tcp_timeout_lambda: |-
  // partial is the timed-out input chunk
  if (partial.size() < 3) return std::string();
  return partial;
```

## `rolloffino`

`rolloffino` exposes a Rolloffino-compatible TCP protocol for roof open/close control using two output pins and two limit sensors.

The `indi-rolloffino` driver is a popular third-party [INDI](https://indilib.org/) driver designed for Arduino-based, roll-off roof observatory controllers, often utilizing linear actuators. It operates within the `indi-3rdparty` [repository](https://github.com/indilib/indi-3rdparty), which houses drivers that have external dependencies or are maintained by community members rather than the core INDI team.

### Rolloffino Options

| Key | Type | Default | Description |
|---|---|---|---|
| `id` | id | required | Component ID. |
| `in1` | internal GPIO output | required | Motor driver IN1 pin. |
| `in2` | internal GPIO output | required | Motor driver IN2 pin. |
| `opened_limit_pin` | GPIO input | optional | GPIO pin wired to the opened limit switch. |
| `closed_limit_pin` | GPIO input | optional | GPIO pin wired to the closed limit switch. |
| `duty_cycle` | int 0..100 | `100` | Duty cycle percentage (reserved for future PWM use). |
| `max_duration` | duration | `30s` | Maximum movement time before abort. |

`rolloffino` also accepts all shared `tcp_server` options.

If neither `opened_limit_pin` nor `closed_limit_pin` is configured for a given direction, that limit is treated as never reached (no interlock for that direction).

#### `opened_limit_pin` / `closed_limit_pin` defaults

Limit switch pins are normally-closed (NC) switches and are pre-configured with sensible defaults:
- `inverted: true` — switch at rest reads `HIGH`; tripped reads `true`
- `mode.input: true`
- `mode.pullup: true`

You can override any of these defaults with a full pin spec. Always specify pin using the `number:` key.

### Example — compact (GPIO limit pins, recommended)

```yaml
rolloffino:
  id: roof
  port: 8888
  in1: D5
  in2: D6
  opened_limit_pin:
    number: D1    # inverted+pullup applied automatically
  closed_limit_pin:
    number: D2
  max_duration: 30s
  tcp_terminator: ")"
```

### Example — with overridden pin spec

```yaml
rolloffino:
  id: roof
  port: 8888
  in1: D5
  in2: D6
  opened_limit_pin:
    number: D1
    inverted: false   # NO switch instead of NC
    mode:
      input: true
      pullup: false
  closed_limit_pin:
    number: D2
  max_duration: 30s
  tcp_terminator: ")"
```

## `snapcap`

`snapcap` implements a TCP protocol for dust-cap/flat-panel style devices with servo position and light state control.

The [INDI](https://indilib.org/) SnapCap driver is a software component in the INDI (Instrument-Neutral Distributed Interface) system. It manages a motorized telescope dust cover and light source, known as a "Gemini SnapCap". It allows astrophotography software, like KStars/Ekos, to automatically open, close, and use the cap for calibration flat frames.

### SnapCap Options

| Key | Type | Default | Description |
|---|---|---|---|
| `id` | id | required | Component ID. |
| `servo_id` | servo id | required | Servo used for cap movement. |
| `device_id` | enum/int | `FLIP_FLAT` (`99`) | Protocol device identifier. |
| `brightness` | int 0..255 | `128` | Initial brightness value. |
| `max_degrees` | int 1..999 | `270` | Maximum logical servo range. |
| `position` | int 0..999 | `0` | Initial logical position (runtime clamped to `0..max_degrees`). |

`snapcap` also accepts all shared `tcp_server` options.

### `snapcap.device_id` accepted values

| Name | Value |
|---|---|
| `FLAT_MAN_L` | `10` |
| `FLAT_MAN_XL` | `15` |
| `FLAT_MAN` | `19` |
| `FLIP_DUST` | `98` |
| `FLIP_FLAT` | `99` |

YAML examples:

```yaml
# Enum form (recommended)
snapcap:
  id: cap
  servo_id: cap_servo
  device_id: FLIP_FLAT
```

```yaml
# Numeric form
snapcap:
  id: cap
  servo_id: cap_servo
  device_id: 99
```

### Example

```yaml
servo:
  - id: cap_servo
    pin: GPIO5
    auto_detach_time: 0ms

snapcap:
  id: cap
  servo_id: cap_servo
  device_id: FLIP_FLAT
  max_degrees: 270
  brightness: 128
  position: 0
  port: 9999
  tcp_terminator: "\r"

number:
  - platform: snapcap
    snapcap_id: cap
    name: "SnapCap Servo Position"
    step: 1
```

For protocol details and command tables, see `components/snapcap/README.md`.

## Notes

- `tcp_server` is intentionally the shared base for protocol behavior; use derived components for device-specific commands.
- For protocol changes, keep shared socket/buffer behavior in `components/tcp_server/tcp_server.cpp` and command handling in derived components.
- This repository still contains a `line_server` directory for historical compatibility, but active development is centered on `tcp_server` and its subclasses.
