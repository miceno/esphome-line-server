import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_PORT,
)
import esphome.components.tcp_server as tcp_server

CONF_IN1_PIN = "in1"
CONF_IN2_PIN = "in2"
CONF_DUTY_CYCLE = "duty_cycle"

CONF_TCP_BUFFER_SIZE = "tcp_buffer_size"
CONF_TCP_TERMINATOR = "tcp_terminator"
CONF_TCP_TIMEOUT = "tcp_timeout"
CONF_TCP_TIMEOUT_LAMBDA = "tcp_timeout_lambda"

CONF_OPENED_SENSOR = "opened_sensor"
CONF_CLOSED_SENSOR = "closed_sensor"

AUTO_LOAD = ["socket"]

DEPENDENCIES = ["network"]

MULTI_CONF = True

RolloffinoComponent = cg.global_ns.class_("RolloffinoComponent", tcp_server.TCPServerComponent)


def validate_buffer_size(buffer_size):
    if buffer_size & (buffer_size - 1) != 0:
        raise cv.Invalid("Buffer size must be a power of two.")
    return buffer_size


def validate_terminator(value):
    value = cv.string(value)
    if len(value.encode("utf-8")) > 4:
        raise cv.Invalid("Terminator must be <= 4 bytes")
    return value


CONFIG_SCHEMA = tcp_server.CONFIG_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RolloffinoComponent),
        cv.Optional(CONF_PORT, default=8888): cv.port,

        cv.Optional(CONF_TCP_BUFFER_SIZE, default=256): cv.All(
            cv.positive_int, validate_buffer_size
            ),
        cv.Optional(CONF_TCP_TERMINATOR, default="\r"): validate_terminator,
        cv.Optional(CONF_TCP_TIMEOUT, default="300ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TCP_TIMEOUT_LAMBDA): cv.returning_lambda,

        cv.Required(CONF_OPENED_SENSOR): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_CLOSED_SENSOR): cv.use_id(binary_sensor.BinarySensor),

        cv.Required(CONF_IN1_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_IN2_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_DUTY_CYCLE, default="100"): cv.int_range(min=0, max=100),
    }
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await tcp_server.to_code(config)
    open_sensor = await cg.get_variable(config[CONF_OPENED_SENSOR])
    cg.add(var.set_opened_binary_sensor(open_sensor))
    closed_sensor = await cg.get_variable(config[CONF_CLOSED_SENSOR])
    cg.add(var.set_closed_binary_sensor(closed_sensor))
    cg.add(var.set_duty_cycle(config[CONF_DUTY_CYCLE]))
    in1_pin = await cg.gpio_pin_expression(config[CONF_IN1_PIN])
    cg.add(var.set_in1_pin(in1_pin))
    in2_pin = await cg.gpio_pin_expression(config[CONF_IN2_PIN])
    cg.add(var.set_in2_pin(in2_pin))
    await cg.register_component(var, config)
