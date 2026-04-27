# SnapCap Cover Component

`snapcap_cover` is a minimal SnapCap-like TCP component for a binary cap actuator:

- `OPEN`
- `CLOSED`

It also exposes a Home Assistant cover entity with assumed state.

It uses a servo as the actuator and tracks state as assumed timing-based state (no position sensor).

## Configuration

```yaml
servo:
  - id: cap_servo
    pin: GPIO5
    auto_detach_time: 0ms

snapcap_cover:
  id: cap_cover
  name: "SnapCap Cover"
  servo_id: cap_servo
  protocol_device_id: FLIP_FLAT
  open_level: 1.0
  closed_level: -1.0
  move_duration: 1200ms
  initial_state: CLOSED
  assumed_open:
    name: "SnapCap Assumed Open"
  state_text:
    name: "SnapCap State"
  port: 9999
  tcp_terminator: "\r"
```

## Options

- `name` (Optional): Name of the HA cover entity.
- `id` (Required): Component id.
- `servo_id` (Required): Servo id used to actuate the cap.
- `protocol_device_id` (Optional, default: `FLIP_FLAT` / `99`): Protocol device id.
- `open_level` (Optional, default: `1.0`): Servo target level for open, range `-1.0..1.0`.
- `closed_level` (Optional, default: `-1.0`): Servo target level for closed, range `-1.0..1.0`.
- `move_duration` (Optional, default: `1200ms`): Assumed movement duration before state becomes stable.
- `initial_state` (Optional, default: `CLOSED`): Initial assumed cap state (`OPEN` or `CLOSED`).
- `assumed_open` (Optional): Exposes a binary sensor with assumed cap state (`true`=open, `false`=closed).
- `state_text` (Optional): Exposes a text sensor with state values: `moving`, `open`, `closed`, `aborted`.

## Home Assistant Cover Behavior

- Cover is reported as assumed state (`is_assumed_state: true`).
- Open/Close commands map to full open/full close servo movement.
- Stop maps to SnapCap abort behavior.

It also supports all shared `tcp_server` options (`port`, `tcp_buffer_size`, `tcp_terminator`, `tcp_timeout`, `tcp_timeout_lambda`).

For compatibility, `device_id` is also accepted and migrated to `protocol_device_id`.

## TCP Commands

Supported protocol subset:

| Command | Response | Effect |
|---|---|---|
| `>O000` | `*O000` | Open cap |
| `>o000` | `*o000` | Force open (same behavior) |
| `>C000` | `*C000` | Close cap |
| `>c000` | `*c000` | Force close (same behavior) |
| `>A000` | `*A000` | Abort (detaches servo, status becomes user abort) |
| `>P000` | `*PII00` | Ping; `II` = two-digit `device_id` |
| `>S000` | `*SMLC` | Status; `M`=servo (0/1), `L` always `0`, `C`=cover status |
| `>V000` | `*V100` | Firmware version |
| `>W000` | `*W000` | Ack only |

Unknown commands return `*ERR`.

