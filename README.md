# ESPHome Components

## Rollofino for ESPHome

**RolloffinoServer** is a custom ESPHome component based on [@oxan’s stream_server](https://github.com/oxan/esphome-stream-server). 
It acts as a [Rolloffino device](https://github.com/indilib/indi-3rdparty/tree/master/indi-rolloffino) suitable to be integrated in [Kstars](https://kstars.kde.org/).

The component listens on a configurable TCP port and accepts the rollofino protocol.

### Features

- TCP server for Rolloffino protocol
- Bi-directional TCP communication
- Compatible with Wi-Fi
- TODO: add `ROOF_MOVEMENT_MIN_TIME_MILLIS` and `ROOF_MOTION_END_DELAY_MILLIS` settings.
- TODO: add support for regulating the speed of each motor to keep them in sync.


### Requirements

- ESPHome version **2022.3.0** or newer

### Installation

```yaml
external_components:
  - source: github://miceno/esphome-line-server
    components: [rolloffino]
```
### Configuration

```yaml

rolloffino:
  port: 8888
  left_motor:
    enable_pin: D5
    direction_pin: D6
  right_motor:
    enable_pin: D7
    direction_pin: D8
  opened_pin: D1
  closed_pin: D2
```

### Configuration Options

| Key             | Type    | Default | Description                                                      |
|-----------------|---------|---------|------------------------------------------------------------------|
| `port`          | integer | `8888`  | TCP server port                                                  |
| `opened_pin`    | gpio    | `"D1"`  | Pin connected to the limit sensor that marks the OPENED position |
| `closed_pin`    | gpio    | `"D2"`  | Pin connected to the limit sensor that marks the CLOSED position |
| `left_motor`    |         |         | Left Motor configuration                                         |
| `direction_pin` | gpio    | `"D8"`  | Pin connected to the direction pin on the motor controller       |
| `enable_pin`    | gpio    | `"D7"`  | Pin connected to the enable pin on the motor controller          |
| `right_motor`   |         |         | Right Motor configuration                                        |
| `direction_pin` | gpio    | `"D5"`  | Pin connected to the direction pin on the motor controller       |
| `enable_pin`    | gpio    | `"D6"`  | Pin connected to the enable pin on the motor controller          |


### Notes

- Motors move in synchrony, there is no independent control for each of them.
- Originally based on [esphome-stream-server](https://github.com/oxan/esphome-stream-server) by @oxan and [esphome-line-server](https://github.com/gstos/esphome-line-server) by @gstos.

## Line Server for ESPHome

**LineServer** is a custom ESPHome component based on [@oxan’s stream_server](https://github.com/oxan/esphome-stream-server). 
It acts as a transparent UART-to-TCP line-oriented bridge, 
with extended support for line terminators, timeouts, and directional buffers. 
This makes it ideal for RS-232-based command protocols like RIO (Russound), 
which use line-based communication patterns.

The component listens on a configurable TCP port and forwards lines between UART and TCP clients, 
flushing on a terminator or idle timeout.

---

### Features

- Bi-directional UART–TCP communication
- Independent ring buffers for UART and TCP
- Configurable line terminators for each direction
- Idle flush timeout to handle incomplete lines
- Optional lambda handlers for custom processing of incomplete lines
- TCP client tracking with optional sensors
- Multiple UARTs supported
- Compatible with Wi-Fi and Ethernet

---

### Requirements

- ESPHome version **2022.3.0** or newer

---

### Installation

```yaml
external_components:
  - source: github://gstos/esphome-line-server
    components: [line_server]
```

### Basic Usage

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

line_server:
  uart_id: uart_bus
```

### Configuration Options

| Key                   | Type              | Default | Description                                                  |
|-----------------------|-------------------|---------|--------------------------------------------------------------|
| `port`                | integer           | `6638`  | TCP server port                                              |
| `uart_terminator`     | string            | `"\r\n"`| Terminator to flush UART buffer to TCP                       |
| `uart_buffer_size`    | power of 2 int    | `256`   | Buffer size for UART input (in addition to RX buffer)        |
| `uart_timeout`        | duration          | `500ms` | Time before incomplete UART messages are flushed             |
| `uart_timeout_lambda` | lambda            | emtpy   | Hook for addressing of incomplete content received from UART |
| `tcp_buffer_size`     | power of 2 int    | `256`   | Buffer size for TCP input                                    |
| `tcp_terminator`      | string            | `"\r"`  | Terminator to flush TCP buffer to UART                       |
| `tcp_timeout`         | duration          | `300ms` | Time before incomplete TCP messages are flushed              |
| `tcp_timeout_lambda`  | lambda            | emtpy   | Hook for addressing of incomplete content received from TCP  |

#### Example with all options:

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

line_server:
  uart_id: uart_bus
  port: 7000                        # TCP port (default: 6638)
  uart_buffer_size: 512            # Must be power of 2
  tcp_buffer_size: 512             # Must be power of 2
  uart_terminator: "\r\n"          # Flush UART buffer on this sequence
  tcp_terminator: "\r"             # Flush TCP buffer on this sequence
  uart_timeout: 500ms              # Flush UART partial line after idle
  tcp_timeout: 300ms               # Flush TCP partial line after idle

  uart_timeout_lambda: |-
    return "[UART TIMEOUT]";       # Optional: override stale UART line

  tcp_timeout_lambda: |-
    return "[TCP TIMEOUT]";        # Optional: override stale TCP line
```

#### Example: Timeout Lambda for Incomplete Lines

You can use a lambda to **process, modify, or preserve** partial messages that timeout without a terminator.

##### Forward the partial message as-is:

```yaml
line_server:
  uart_id: uart_bus
  uart_timeout_lambda: |-
    return partial;  # Just forward the partial line
```

##### Add a suffix to incomplete messages:

```yaml
line_server:
  uart_id: uart_bus
  uart_timeout_lambda: |-
    return partial + " [incomplete]";
```

##### Drop very short lines:

```yaml
line_server:
  uart_id: uart_bus
  uart_timeout_lambda: |-
    if (partial.length() < 5) return "";  // discard
    return partial;
```

### Sensors

#### Binary Sensor: Client Connected

```yaml
binary_sensor:
  - platform: line_server
    connected:
      name: TCP Client Connected
```

#### Sensor: Connection Count
```yaml
sensor:
  - platform: line_server
    connections:
      name: TCP Client Count
```

### Multiple UARTs

You can use multiple UARTs with separate line servers:

```yaml
uart:
  - id: uart1
    rx_pin: GPIO16
    tx_pin: GPIO17
    baud_rate: 9600

  - id: uart2
    rx_pin: GPIO25
    tx_pin: GPIO26
    baud_rate: 19200

line_server:
  - uart_id: uart1
    port: 7001

  - uart_id: uart2
    port: 7002
```

### Notes

- Buffer sizes must be **powers of two**.
- Terminators must be **≤ 4 bytes**, UTF-8 encoded.
- The `*_timeout_lambda` allows customizing what is sent when an incomplete line is flushed. 
  Returning an empty string means the data is discarded.
- All data is treated as **raw** — no Telnet, RFC2217, or control sequences.
- Notice that the UART buffer size implements an additional buffer on top of the ESPHome RX buffer.
- For now, it is up for the consumer of the TCP API to handle "request -> response" cycles correctly. 
  In the future, we may implement some sort of state handling to properly manage "request -> response" cycles correctly.
  Have that in mind, specially if you are sending requests from multiple clients to the same UART.
- Notice that the default behaviour is to **flush** the buffers on a timeout.
  Consider this behaviour for protocols that expect a response to a command.
- Originally based on [esphome-stream-server](https://github.com/oxan/esphome-stream-server) by @oxan.
