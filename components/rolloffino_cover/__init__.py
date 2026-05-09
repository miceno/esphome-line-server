import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor, cover
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_MAX_DURATION,
    CONF_PORT,
    CONF_INVERTED,
    CONF_NUMBER,
    CONF_MODE,
    CONF_OUTPUT
)
import esphome.components.tcp_server as tcp_server

CONF_IN1_PIN = "in1"
CONF_IN2_PIN = "in2"
CONF_DUTY_CYCLE = "duty_cycle"

CONF_OPENED_SENSOR = "opened_sensor"
CONF_CLOSED_SENSOR = "closed_sensor"

AUTO_LOAD = ["tcp_server", "cover"]
DEPENDENCIES = ["tcp_server"]

_LOGGER = logging.getLogger(__name__)

MULTI_CONF = True

rolloffino_cover_ns = cg.esphome_ns.namespace("rolloffino_cover")
RolloffinoCoverComponent = rolloffino_cover_ns.class_(
    "RolloffinoCoverComponent", tcp_server.TCPServerComponent, cover.Cover
)


# Validate only the rolloffino-specific schema additions and reuse cover schema
def _add_defaults(config):
    # Ensure a sensible default device_class for the cover entity if the user
    # didn't provide one. "shutter" matches a roof/sliding cover semantics.
    if "device_class" not in config:
        config["device_class"] = "shutter"
    return config

# Validate limit output pin config, applying NC switch defaults
def _limit_output_pin_schema(value):
    """Normalize limit output pin config, applying NC switch defaults:
    inverted=True, mode.output=True
    Accepts a bare GPIO number/name or a full pin spec dict."""
    if isinstance(value, (int, str)):
        value = {CONF_NUMBER: value}
    value = dict(value)
    value.setdefault(CONF_INVERTED, True)
    mode = dict(value.get(CONF_MODE, {}))
    # Require output mode for limit outputs, but don't force pullup.
    # For ESP8266 some pins/modes reject an output+pullup combination,
    # so leave pullup alone unless user explicitly sets it.
    mode.setdefault(CONF_OUTPUT, True)
    value[CONF_MODE] = mode
    _LOGGER.info(f"Normalized output pin config: {value}")
    return pins.internal_gpio_output_pin_schema(value)


LIMIT_OUTPUT_PIN_SCHEMA = _limit_output_pin_schema


CONFIG_SCHEMA = cv.All(
    cover.cover_schema(RolloffinoCoverComponent).extend(
        {
            cv.Required(CONF_OPENED_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_CLOSED_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_IN1_PIN): LIMIT_OUTPUT_PIN_SCHEMA,
            cv.Required(CONF_IN2_PIN): LIMIT_OUTPUT_PIN_SCHEMA,
            cv.Optional(CONF_DUTY_CYCLE, default="100"): cv.int_range(min=0, max=100),
            cv.Optional(CONF_MAX_DURATION, default="30s"): cv.positive_time_period_seconds,
        }
    )
    .extend(tcp_server.TCP_SERVER_SCHEMA),
    _add_defaults,
)


async def to_code(config):
    # Create the new RolloffinoCoverComponent instance
    var = cg.new_Pvariable(config[CONF_ID])
    # Setup the parent component (tcp server behaviour)
    await tcp_server.setup_tcp_server(var, config)

    # Register as a cover and as an ESPHome component
    await cover.register_cover(var, config)
    await cg.register_component(var, config)

    open_sensor = await cg.get_variable(config[CONF_OPENED_SENSOR])
    cg.add(var.set_opened_binary_sensor(open_sensor))
    closed_sensor = await cg.get_variable(config[CONF_CLOSED_SENSOR])
    cg.add(var.set_closed_binary_sensor(closed_sensor))
    cg.add(var.set_duty_cycle(config[CONF_DUTY_CYCLE]))
    in1_pin = await cg.gpio_pin_expression(config[CONF_IN1_PIN])
    cg.add(var.set_in1_pin(in1_pin))
    in2_pin = await cg.gpio_pin_expression(config[CONF_IN2_PIN])
    cg.add(var.set_in2_pin(in2_pin))
    if CONF_MAX_DURATION in config:
        cg.add(var.set_max_duration(config[CONF_MAX_DURATION]))






