import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_MAX_DURATION,
    CONF_PORT,
)
import esphome.components.tcp_server as tcp_server

CONF_IN1_PIN = "in1"
CONF_IN2_PIN = "in2"
CONF_DUTY_CYCLE = "duty_cycle"

CONF_OPENED_SENSOR = "opened_sensor"
CONF_CLOSED_SENSOR = "closed_sensor"
CONF_OPENED_LIMIT_PIN = "opened_limit_pin"
CONF_CLOSED_LIMIT_PIN = "closed_limit_pin"

AUTO_LOAD = ["tcp_server"]
DEPENDENCIES = ["tcp_server"]


def _limit_switch_pin_schema(value):
    """Normalize limit switch pin config, applying NC switch defaults:
    inverted=True, mode.input=True, mode.pullup=True.
    Accepts a bare GPIO number or a full pin spec dict."""
    if isinstance(value, int):
        value = {"number": value}
    value = dict(value)
    value.setdefault("inverted", True)
    mode = dict(value.get("mode", {}))
    mode.setdefault("input", True)
    mode.setdefault("pullup", True)
    value["mode"] = mode
    return pins.internal_gpio_input_pin_schema(value)


LIMIT_SWITCH_PIN_SCHEMA = _limit_switch_pin_schema

MULTI_CONF = True

rolloffino_ns = cg.esphome_ns.namespace("rolloffino")
RolloffinoComponent = rolloffino_ns.class_("RolloffinoComponent",
                                              tcp_server.TCPServerComponent,
                                              cg.Component)


# Validate only the rolloffino-specific schema additions
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(RolloffinoComponent),

    cv.Optional(CONF_OPENED_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_CLOSED_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_OPENED_LIMIT_PIN): LIMIT_SWITCH_PIN_SCHEMA,
    cv.Optional(CONF_CLOSED_LIMIT_PIN): LIMIT_SWITCH_PIN_SCHEMA,
    cv.Required(CONF_IN1_PIN): pins.internal_gpio_output_pin_schema,
    cv.Required(CONF_IN2_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_DUTY_CYCLE, default="100"): cv.int_range(min=0, max=100),
    cv.Optional(CONF_MAX_DURATION, default="30s"): cv.positive_time_period_seconds,
}).extend(tcp_server.TCP_SERVER_SCHEMA)


async def to_code(config):
    # Create the new RolloffinoComponent instance
    var = cg.new_Pvariable(config[CONF_ID])
    # Setup the parent component
    await tcp_server.setup_tcp_server(var, config)
    await cg.register_component(var, config)

    # Configure limit detection: prefer GPIO pins, fall back to external sensors
    if CONF_OPENED_LIMIT_PIN in config:
        opened_limit_pin = await cg.gpio_pin_expression(config[CONF_OPENED_LIMIT_PIN])
        cg.add(var.set_opened_limit_pin(opened_limit_pin))
    elif CONF_OPENED_SENSOR in config:
        open_sensor = await cg.get_variable(config[CONF_OPENED_SENSOR])
        cg.add(var.set_opened_binary_sensor(open_sensor))

    if CONF_CLOSED_LIMIT_PIN in config:
        closed_limit_pin = await cg.gpio_pin_expression(config[CONF_CLOSED_LIMIT_PIN])
        cg.add(var.set_closed_limit_pin(closed_limit_pin))
    elif CONF_CLOSED_SENSOR in config:
        closed_sensor = await cg.get_variable(config[CONF_CLOSED_SENSOR])
        cg.add(var.set_closed_binary_sensor(closed_sensor))

    cg.add(var.set_duty_cycle(config[CONF_DUTY_CYCLE]))
    in1_pin = await cg.gpio_pin_expression(config[CONF_IN1_PIN])
    cg.add(var.set_in1_pin(in1_pin))
    in2_pin = await cg.gpio_pin_expression(config[CONF_IN2_PIN])
    cg.add(var.set_in2_pin(in2_pin))
    if CONF_MAX_DURATION in config:
        cg.add(var.set_max_duration(config[CONF_MAX_DURATION]))
